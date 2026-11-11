/*
 * ipc.c - FR4: inter-process communication with POSIX shared memory.
 *
 * Two cooperating processes:
 *   Controller (parent) - runs a scheduling policy and publishes the per-job
 *                         results in a shared-memory "board".
 *   Reporter   (child)  - attaches to the board BY NAME, verifies a checksum,
 *                         computes its own averages and writes them back.
 *
 * Synchronisation uses two NAMED semaphores, both created with value 0:
 *   SEM_REQ_NAME : controller posts -> "results are on the board"
 *   SEM_ACK_NAME : reporter posts   -> "my reply is on the board"
 * The semaphores enforce strict alternation, so the board is never accessed
 * by both processes at the same time and needs no extra lock. All waits use
 * sem_timedwait() so a crashed peer can never hang the program.
 *
 * Cleanup: munmap, close, sem_close, then shm_unlink / sem_unlink for the
 * three names defined in common.h - on success AND on every error path.
 */
#include "ipc.h"
#include "common.h"
#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define WAIT_SECONDS 5

typedef struct {
    int  id;
    char name[MAX_NAME_LEN];
    int  arrival, burst, completion, turnaround, waiting;
} SharedRecord;

typedef struct {
    /* written by the controller */
    int          controller_pid;
    char         policy[32];
    int          count;
    SharedRecord rec[MAX_JOBS];
    unsigned int checksum;
    /* written by the reporter */
    int          reporter_pid;
    int          reporter_count;
    unsigned int reporter_checksum;
    double       reporter_avg_wt;
    double       reporter_avg_tat;
} SharedBoard;

typedef struct {
    int          fd;
    SharedBoard *board;
    sem_t       *req;
    sem_t       *ack;
} IpcHandles;

static void handles_init(IpcHandles *h)
{
    h->fd = -1;
    h->board = NULL;
    h->req = SEM_FAILED;
    h->ack = SEM_FAILED;
}

/* Detach from the objects; optionally also remove their names system-wide. */
static void handles_release(IpcHandles *h, int do_unlink, int verbose)
{
    if (h->board != NULL) {
        munmap(h->board, sizeof(SharedBoard));
        h->board = NULL;
        if (verbose) printf("  [cleanup] munmap() detached the shared-memory mapping\n");
    }
    if (h->fd >= 0) {
        close(h->fd);
        h->fd = -1;
        if (verbose) printf("  [cleanup] close() released the shared-memory descriptor\n");
    }
    if (h->req != SEM_FAILED) {
        sem_close(h->req);
        h->req = SEM_FAILED;
    }
    if (h->ack != SEM_FAILED) {
        sem_close(h->ack);
        h->ack = SEM_FAILED;
        if (verbose) printf("  [cleanup] sem_close() closed both semaphore handles\n");
    }
    if (do_unlink)
        ipc_unlink_all(verbose);
}

static int sem_wait_timeout(sem_t *s, int seconds)
{
    struct timespec ts;
    int rc;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += seconds;
    do {
        rc = sem_timedwait(s, &ts);
    } while (rc == -1 && errno == EINTR);
    return rc;
}

static unsigned int board_checksum(const SharedBoard *b)
{
    unsigned int h = 2166136261u;                 /* FNV-1a */
#define MIX(v) (h = (h ^ (unsigned int)(v)) * 16777619u)
    MIX(b->count);
    for (int i = 0; i < b->count; i++) {
        const SharedRecord *r = &b->rec[i];

        MIX(r->id); MIX(r->arrival); MIX(r->burst);
        MIX(r->completion); MIX(r->turnaround); MIX(r->waiting);
        for (const char *c = r->name; *c; c++)
            MIX(*c);
    }
#undef MIX
    return h;
}

/* ----------------------- object existence / removal ---------------------- */

static int shm_exists(void)
{
    int fd = shm_open(SHM_NAME, O_RDONLY, 0);

    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

static int sem_exists(const char *name)
{
    sem_t *s = sem_open(name, 0);

    if (s == SEM_FAILED)
        return 0;
    sem_close(s);
    return 1;
}

int ipc_objects_present(int verbose)
{
    int shm = shm_exists(), req = sem_exists(SEM_REQ_NAME), ack = sem_exists(SEM_ACK_NAME);

    if (verbose) {
        printf("  %-24s : %s\n", SHM_NAME, shm ? "PRESENT" : "absent");
        printf("  %-24s : %s\n", SEM_REQ_NAME, req ? "PRESENT" : "absent");
        printf("  %-24s : %s\n", SEM_ACK_NAME, ack ? "PRESENT" : "absent");
    }
    return shm + req + ack;
}

int ipc_unlink_all(int verbose)
{
    int removed = 0;

    if (shm_unlink(SHM_NAME) == 0) {
        removed++;
        if (verbose) printf("  [cleanup] shm_unlink(%s) removed the shared-memory object\n", SHM_NAME);
    }
    if (sem_unlink(SEM_REQ_NAME) == 0) {
        removed++;
        if (verbose) printf("  [cleanup] sem_unlink(%s) removed the semaphore\n", SEM_REQ_NAME);
    }
    if (sem_unlink(SEM_ACK_NAME) == 0) {
        removed++;
        if (verbose) printf("  [cleanup] sem_unlink(%s) removed the semaphore\n", SEM_ACK_NAME);
    }
    return removed;
}

/* ------------------------------ reporter -------------------------------- */

/* Child process: attaches to the existing objects purely by name. */
static int reporter_main(void)
{
    IpcHandles h;
    SharedBoard *b;
    double sum_wt = 0, sum_tat = 0;
    unsigned int sum;
    int rc = 1;

    handles_init(&h);
    printf("[reporter pid=%d] started; attaching to %s and the semaphores by name\n",
           (int)getpid(), SHM_NAME);
    fflush(stdout);

    h.fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (h.fd < 0) {
        print_error("[reporter] shm_open: %s", strerror(errno));
        goto out;
    }
    b = mmap(NULL, sizeof(SharedBoard), PROT_READ | PROT_WRITE, MAP_SHARED, h.fd, 0);
    if (b == MAP_FAILED) {
        print_error("[reporter] mmap: %s", strerror(errno));
        goto out;
    }
    h.board = b;
    h.req = sem_open(SEM_REQ_NAME, 0);
    h.ack = sem_open(SEM_ACK_NAME, 0);
    if (h.req == SEM_FAILED || h.ack == SEM_FAILED) {
        print_error("[reporter] sem_open: %s", strerror(errno));
        goto out;
    }

    printf("[reporter pid=%d] waiting on %s (sem_wait)...\n", (int)getpid(), SEM_REQ_NAME);
    fflush(stdout);
    if (sem_wait_timeout(h.req, WAIT_SECONDS) != 0) {
        print_error("[reporter] timed out waiting for the controller: %s", strerror(errno));
        goto out;
    }

    sum = board_checksum(b);
    b->reporter_pid = (int)getpid();
    b->reporter_checksum = sum;
    printf("[reporter pid=%d] woke up: %d record(s) from controller pid=%d, policy %s\n",
           (int)getpid(), b->count, b->controller_pid, b->policy);
    if (sum != b->checksum) {
        printf("[reporter pid=%d] CHECKSUM MISMATCH (computed 0x%08x, published 0x%08x)\n",
               (int)getpid(), sum, b->checksum);
        b->reporter_count = -1;
    } else {
        for (int i = 0; i < b->count; i++) {
            sum_wt += b->rec[i].waiting;
            sum_tat += b->rec[i].turnaround;
            printf("[reporter pid=%d]   J%-3d %-16s CT=%-4d TAT=%-4d WT=%-4d\n", (int)getpid(),
                   b->rec[i].id, b->rec[i].name, b->rec[i].completion,
                   b->rec[i].turnaround, b->rec[i].waiting);
        }
        b->reporter_count = b->count;
        b->reporter_avg_wt = sum_wt / b->count;
        b->reporter_avg_tat = sum_tat / b->count;
        printf("[reporter pid=%d] checksum 0x%08x verified; computed avg WT=%.2f avg TAT=%.2f\n",
               (int)getpid(), sum, b->reporter_avg_wt, b->reporter_avg_tat);
        rc = 0;
    }
    fflush(stdout);
    log_event("IPC", "reporter verified %d record(s), checksum 0x%08x", b->count, sum);
    sem_post(h.ack);                      /* hand the board back */

out:
    handles_release(&h, 0, 0);            /* the child never unlinks names */
    return rc;
}

/* ------------------------------ controller ------------------------------ */

int ipc_demo(const char *jobfile, Policy policy, int quantum, int verbose)
{
    JobSet set;
    SchedResult *res = NULL;
    IpcHandles h;
    SharedBoard *b;
    pid_t child = -1;
    int ok = 0, status = 0, stale;

    (void)verbose;
    handles_init(&h);
    if (jobset_load(jobfile, &set) < 0)
        return 0;
    res = malloc(sizeof *res);
    if (res == NULL || sched_run(policy, &set, quantum, res) != 0) {
        free(res);
        return 0;
    }

    print_section("Creating IPC objects");
    stale = ipc_unlink_all(0);            /* only our own names; left by a crashed run */
    if (stale > 0)
        printf("  (removed %d stale object(s) left by an earlier interrupted run)\n", stale);

    h.fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (h.fd < 0) {
        print_error("shm_open(%s): %s", SHM_NAME, strerror(errno));
        goto done;
    }
    if (ftruncate(h.fd, (off_t)sizeof(SharedBoard)) != 0) {
        print_error("ftruncate: %s", strerror(errno));
        goto done;
    }
    b = mmap(NULL, sizeof(SharedBoard), PROT_READ | PROT_WRITE, MAP_SHARED, h.fd, 0);
    if (b == MAP_FAILED) {
        print_error("mmap: %s", strerror(errno));
        goto done;
    }
    h.board = b;
    memset(b, 0, sizeof *b);
    h.req = sem_open(SEM_REQ_NAME, O_CREAT | O_EXCL, 0600, 0);
    h.ack = sem_open(SEM_ACK_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (h.req == SEM_FAILED || h.ack == SEM_FAILED) {
        print_error("sem_open: %s", strerror(errno));
        goto done;
    }
    printf("  shm_open + ftruncate(%zu bytes) + mmap  -> %s\n", sizeof(SharedBoard), SHM_NAME);
    printf("  sem_open (initial value 0)               -> %s, %s\n", SEM_REQ_NAME, SEM_ACK_NAME);
    printf("  Objects while the exchange is running:\n");
    ipc_objects_present(1);

    print_section("Process exchange");
    fflush(NULL);
    child = fork();
    if (child < 0) {
        print_error("fork: %s", strerror(errno));
        goto done;
    }
    if (child == 0) {
        int rc = reporter_main();

        fflush(stdout);
        _exit(rc);
    }

    /* Controller: publish the results, then signal the reporter. */
    b->controller_pid = (int)getpid();
    snprintf(b->policy, sizeof b->policy, "%s", policy_short(policy));
    b->count = res->n;
    for (int i = 0; i < res->n; i++) {
        const Job *j = &res->jobs[i];
        SharedRecord *r = &b->rec[i];

        r->id = j->id;
        snprintf(r->name, sizeof r->name, "%s", j->name);
        r->arrival = j->arrival;
        r->burst = j->burst;
        r->completion = j->completion;
        r->turnaround = j->turnaround;
        r->waiting = j->waiting;
    }
    b->checksum = board_checksum(b);
    printf("[controller pid=%d] scheduled %d job(s) with %s; wrote them to shared memory "
           "(checksum 0x%08x)\n", (int)getpid(), b->count, b->policy, b->checksum);
    printf("[controller pid=%d] sem_post(%s) -> reporter may read\n", (int)getpid(), SEM_REQ_NAME);
    fflush(stdout);
    sem_post(h.req);

    if (sem_wait_timeout(h.ack, WAIT_SECONDS) != 0) {
        print_error("controller timed out waiting for the reporter: %s", strerror(errno));
        kill(child, SIGKILL);
        goto done;
    }
    printf("[controller pid=%d] sem_wait(%s) returned; reporter pid=%d replied\n",
           (int)getpid(), SEM_ACK_NAME, b->reporter_pid);

    if (waitpid(child, &status, 0) < 0) {
        print_error("waitpid: %s", strerror(errno));
        child = -1;
        goto done;
    }
    child = -1;

    {
        int count_ok = b->reporter_count == b->count;
        int sum_ok = b->reporter_checksum == b->checksum;
        int avg_ok = fabs(b->reporter_avg_wt - res->avg_wt) < 1e-9 &&
                     fabs(b->reporter_avg_tat - res->avg_tat) < 1e-9;
        int exit_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

        print_section("Verification");
        printf("  Records seen by reporter : %d of %d  %s\n", b->reporter_count, b->count,
               count_ok ? "OK" : "MISMATCH");
        printf("  Checksum agreement       : 0x%08x vs 0x%08x  %s\n", b->checksum,
               b->reporter_checksum, sum_ok ? "OK" : "MISMATCH");
        printf("  Average WT (controller %.2f / reporter %.2f)  %s\n", res->avg_wt,
               b->reporter_avg_wt, avg_ok ? "OK" : "MISMATCH");
        printf("  Reporter exit status     : %s\n", exit_ok ? "exited normally (0)" : "abnormal");
        ok = count_ok && sum_ok && avg_ok && exit_ok;
    }

done:
    if (child > 0) {                      /* never leave a child behind */
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
    }
    print_section("Cleanup of IPC resources");
    handles_release(&h, 1, 1);
    {
        int left = ipc_objects_present(1);

        printf("  Verified: %s\n", left == 0 ? "no program-created IPC objects remain"
                                              : "SOME OBJECTS REMAIN");
        ok = ok && left == 0;
    }
    log_event("IPC", "exchange %s (policy %s)", ok ? "succeeded" : "FAILED", policy_short(policy));
    free(res);
    return ok;
}

static void usage_ipc(FILE *out)
{
    fprintf(out,
            "usage: campuscompute ipc [jobfile] [--policy fcfs|rr|priority] [--quantum N]\n"
            "  Controller process schedules the jobs and publishes the results in POSIX shared\n"
            "  memory; a forked reporter process verifies them, coordinated by named semaphores.\n"
            "  Defaults: jobfile %s, policy rr, quantum %d.\n", DEFAULT_JOBS_FILE, DEFAULT_QUANTUM);
}

int run_ipc_cmd(int argc, char **argv)
{
    const char *path = DEFAULT_JOBS_FILE;
    Policy policy = POLICY_RR;
    int quantum = DEFAULT_QUANTUM, have_file = 0, ok;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage_ipc(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--policy") == 0) {
            if (i + 1 >= argc || policy_parse(argv[i + 1], &policy) != 0) {
                print_error("'--policy' needs one of fcfs, rr, priority");
                usage_ipc(stderr);
                return EXIT_USAGE;
            }
            i++;
        } else if (strcmp(argv[i], "--quantum") == 0) {
            if (i + 1 >= argc || parse_int_arg(argv[i + 1], 1, MAX_QUANTUM, &quantum) != 0) {
                print_error("'--quantum' needs an integer 1-%d", MAX_QUANTUM);
                usage_ipc(stderr);
                return EXIT_USAGE;
            }
            i++;
        } else if (argv[i][0] == '-') {
            print_error("unknown option '%s'", argv[i]);
            usage_ipc(stderr);
            return EXIT_USAGE;
        } else if (!have_file) {
            path = argv[i];
            have_file = 1;
        } else {
            print_error("unexpected extra argument '%s'", argv[i]);
            usage_ipc(stderr);
            return EXIT_USAGE;
        }
    }

    print_banner("Shared-memory IPC with named semaphores");
    printf("  Job file: %s   Policy: %s%s\n", path, policy_short(policy),
           policy == POLICY_RR ? " (quantum shown in the result)" : "");
    ok = ipc_demo(path, policy, quantum, 1);
    printf("\nIPC result: %s\n", ok ? "SUCCESS" : "FAILURE");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
