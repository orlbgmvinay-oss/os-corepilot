/*
 * worker.c - FR6: worker-failure detection.
 *
 * A supervisor (the parent) forks CPU-bound worker processes and polls them
 * with waitpid(pid, &status, WNOHANG). The wait status is decoded with
 * WIFEXITED / WEXITSTATUS / WIFSIGNALED / WTERMSIG, so the report states
 * exactly how each child ended.
 *
 * Failures are SIMULATED fault injections, clearly labelled in the output:
 *   fail  - the worker calls _exit(3)                (non-zero exit status)
 *   crash - the worker calls abort()                 (killed by SIGABRT)
 *   hang  - the worker blocks forever; the supervisor's watchdog notices it
 *           has not finished within the timeout and sends SIGKILL.
 * The watchdog's SIGKILL is a real kill of a (simulated) hung worker.
 */
#include "worker.h"
#include "common.h"
#include "logger.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORKERS     8
#define SIM_EXIT_CODE   3
#define POLL_INTERVAL   20      /* milliseconds */

typedef enum { WK_NORMAL, WK_FAIL_EXIT, WK_FAIL_SIGNAL, WK_HANG } WorkerKind;

typedef struct {
    int        id;               /* 1-based label (W1, W2, ...)            */
    WorkerKind kind;
    pid_t      pid;
    int        started;
    int        reaped;
    int        status;           /* raw wait status                        */
    int        watchdog_killed;
    double     elapsed;
} WorkerRec;

static const char *kind_description(WorkerKind k)
{
    switch (k) {
    case WK_NORMAL:      return "none (normal job)";
    case WK_FAIL_EXIT:   return "SIMULATED: exit(3)";
    case WK_FAIL_SIGNAL: return "SIMULATED: abort()";
    case WK_HANG:        return "SIMULATED: hang";
    }
    return "?";
}

static long count_primes(long limit)
{
    long count = 0;

    for (long n = 2; n < limit; n++) {
        int prime = 1;

        for (long d = 2; d * d <= n; d++)
            if (n % d == 0) {
                prime = 0;
                break;
            }
        count += prime;
    }
    return count;
}

/* Runs in the forked child; never returns. */
static void worker_child(int id, WorkerKind kind)
{
    struct rlimit no_core = { 0, 0 };
    long limit = 20000 + 5000L * id;
    long primes;

    setrlimit(RLIMIT_CORE, &no_core);            /* abort() must not dump core */
    primes = count_primes(limit);
    printf("[W%d pid=%d] job started: primes below %ld = %ld\n", id, (int)getpid(), limit, primes);
    fflush(stdout);

    switch (kind) {
    case WK_NORMAL:
        printf("[W%d pid=%d] completed successfully\n", id, (int)getpid());
        fflush(stdout);
        _exit(0);
    case WK_FAIL_EXIT:
        printf("[W%d pid=%d] *** SIMULATED FAILURE *** exiting with status %d\n", id,
               (int)getpid(), SIM_EXIT_CODE);
        fflush(stdout);
        _exit(SIM_EXIT_CODE);
    case WK_FAIL_SIGNAL:
        printf("[W%d pid=%d] *** SIMULATED CRASH *** calling abort()\n", id, (int)getpid());
        fflush(stdout);
        abort();
    case WK_HANG:
        printf("[W%d pid=%d] *** SIMULATED HANG *** blocking forever\n", id, (int)getpid());
        fflush(stdout);
        for (;;)
            pause();
    }
    _exit(0);
}

static int build_plan(const char *mode, WorkerKind *kinds)
{
    if (strcmp(mode, "normal") == 0) { kinds[0] = WK_NORMAL; return 1; }
    if (strcmp(mode, "fail") == 0)   { kinds[0] = WK_FAIL_EXIT; return 1; }
    if (strcmp(mode, "crash") == 0)  { kinds[0] = WK_FAIL_SIGNAL; return 1; }
    if (strcmp(mode, "hang") == 0)   { kinds[0] = WK_HANG; return 1; }
    if (strcmp(mode, "mix") == 0) {
        kinds[0] = WK_NORMAL;
        kinds[1] = WK_FAIL_EXIT;
        kinds[2] = WK_NORMAL;
        kinds[3] = WK_FAIL_SIGNAL;
        kinds[4] = WK_HANG;
        return 5;
    }
    return -1;
}

static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void describe_status(const WorkerRec *w, char *buf, size_t len)
{
    if (!w->started)
        snprintf(buf, len, "never started (fork failed)");
    else if (WIFEXITED(w->status))
        snprintf(buf, len, "exited normally, code %d", WEXITSTATUS(w->status));
    else if (WIFSIGNALED(w->status))
        snprintf(buf, len, "killed by signal %d (%s)%s", WTERMSIG(w->status),
                 strsignal(WTERMSIG(w->status)), w->watchdog_killed ? " by watchdog" : "");
    else
        snprintf(buf, len, "unexpected wait status 0x%x", w->status);
}

static int is_success(const WorkerRec *w)
{
    return w->started && WIFEXITED(w->status) && WEXITSTATUS(w->status) == 0;
}

/* Did the supervisor observe exactly what was injected? */
static int outcome_matches(const WorkerRec *w)
{
    if (!w->started)
        return 0;
    switch (w->kind) {
    case WK_NORMAL:
        return is_success(w) && !w->watchdog_killed;
    case WK_FAIL_EXIT:
        return WIFEXITED(w->status) && WEXITSTATUS(w->status) == SIM_EXIT_CODE;
    case WK_FAIL_SIGNAL:
        return WIFSIGNALED(w->status) && WTERMSIG(w->status) == SIGABRT;
    case WK_HANG:
        return w->watchdog_killed && WIFSIGNALED(w->status) && WTERMSIG(w->status) == SIGKILL;
    }
    return 0;
}

int worker_scenario(const char *mode, int timeout_seconds, int verbose)
{
    WorkerKind kinds[MAX_WORKERS];
    WorkerRec w[MAX_WORKERS];
    int n = build_plan(mode, kinds);
    int remaining = 0, watchdog_fired = 0, matched = 0, ok_count = 0, fail_count = 0;
    double start;

    (void)verbose;
    if (n < 0) {
        print_error("unknown worker mode '%s'", mode);
        return 0;
    }
    memset(w, 0, sizeof w);

    print_section("Spawning workers");
    printf("  Supervisor pid=%d; watchdog timeout %d s; failures below are SIMULATED, not real OS faults.\n",
           (int)getpid(), timeout_seconds);
    start = now_seconds();
    for (int i = 0; i < n; i++) {
        w[i].id = i + 1;
        w[i].kind = kinds[i];
        fflush(NULL);                              /* no duplicated buffered output */
        w[i].pid = fork();
        if (w[i].pid < 0) {
            print_error("fork failed for W%d: %s", w[i].id, strerror(errno));
            w[i].reaped = 1;
            continue;
        }
        if (w[i].pid == 0)
            worker_child(w[i].id, w[i].kind);
        w[i].started = 1;
        remaining++;
        log_event("WORKER", "spawned W%d pid=%d scenario=%s", w[i].id, (int)w[i].pid,
                  kind_description(w[i].kind));
    }

    while (remaining > 0) {
        for (int i = 0; i < n; i++) {
            int st;
            pid_t r;

            if (!w[i].started || w[i].reaped)
                continue;
            r = waitpid(w[i].pid, &st, WNOHANG);
            if (r == w[i].pid) {
                w[i].status = st;
                w[i].reaped = 1;
                w[i].elapsed = now_seconds() - start;
                remaining--;
            } else if (r < 0 && errno != EINTR) {
                print_error("waitpid(W%d): %s", w[i].id, strerror(errno));
                w[i].status = -1;
                w[i].reaped = 1;
                remaining--;
            }
        }
        if (remaining > 0 && !watchdog_fired && now_seconds() - start >= timeout_seconds) {
            watchdog_fired = 1;
            for (int i = 0; i < n; i++) {
                if (w[i].started && !w[i].reaped) {
                    printf("[supervisor] WATCHDOG: W%d (pid %d) did not finish within %d s -> SIGKILL\n",
                           w[i].id, (int)w[i].pid, timeout_seconds);
                    fflush(stdout);
                    w[i].watchdog_killed = 1;
                    kill(w[i].pid, SIGKILL);
                    log_event("WORKER", "watchdog killed W%d pid=%d", w[i].id, (int)w[i].pid);
                }
            }
        }
        if (remaining > 0)
            sleep_ms(POLL_INTERVAL);
    }

    print_section("Supervisor report (decoded from waitpid status)");
    printf("  %-6s %-8s %-20s %-46s %s\n", "Worker", "PID", "Injected scenario",
           "Observed termination", "Verdict");
    for (int i = 0; i < n; i++) {
        char what[96];

        describe_status(&w[i], what, sizeof what);
        printf("  W%-5d %-8d %-20s %-46s %s\n", w[i].id, (int)w[i].pid,
               kind_description(w[i].kind), what, is_success(&w[i]) ? "OK" : "FAILED");
        log_event("WORKER", "W%d pid=%d %s -> %s", w[i].id, (int)w[i].pid, what,
                  is_success(&w[i]) ? "OK" : "FAILED");
        if (is_success(&w[i])) ok_count++; else fail_count++;
        if (outcome_matches(&w[i])) matched++;
    }

    print_section("Summary");
    printf("  %d worker(s): %d completed OK, %d failed (all failures were SIMULATED fault injections)\n",
           n, ok_count, fail_count);
    printf("  All %d child process(es) were reaped with waitpid(); no zombies remain.\n", n);
    printf("  Detection check: %d of %d outcomes matched what was injected -> %s\n", matched, n,
           matched == n ? "PASS" : "FAIL");
    return matched == n;
}

static void usage_worker(FILE *out)
{
    fprintf(out,
            "usage: campuscompute worker [normal|fail|crash|hang|mix] [--timeout SECONDS]\n"
            "  normal  one worker that finishes with exit status 0\n"
            "  fail    one worker with a SIMULATED failure: exit status 3\n"
            "  crash   one worker with a SIMULATED crash: abort() -> SIGABRT\n"
            "  hang    one worker that SIMULATES a hang; the watchdog kills it\n"
            "  mix     five workers combining all of the above (default)\n"
            "  --timeout SECONDS  watchdog limit, 1-10 (default 1)\n");
}

int run_worker_cmd(int argc, char **argv)
{
    const char *mode = "mix";
    int timeout = 1, have_mode = 0, ok;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage_worker(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 >= argc || parse_int_arg(argv[i + 1], 1, 10, &timeout) != 0) {
                print_error("'--timeout' needs an integer between 1 and 10");
                usage_worker(stderr);
                return EXIT_USAGE;
            }
            i++;
        } else if (argv[i][0] == '-') {
            print_error("unknown option '%s'", argv[i]);
            usage_worker(stderr);
            return EXIT_USAGE;
        } else if (!have_mode) {
            mode = argv[i];
            have_mode = 1;
        } else {
            print_error("unexpected extra argument '%s'", argv[i]);
            usage_worker(stderr);
            return EXIT_USAGE;
        }
    }
    {
        WorkerKind probe[MAX_WORKERS];

        if (build_plan(mode, probe) < 0) {
            print_error("unknown worker mode '%s'", mode);
            usage_worker(stderr);
            return EXIT_USAGE;
        }
    }

    print_banner("Worker processes and failure detection");
    ok = worker_scenario(mode, timeout, 1);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
