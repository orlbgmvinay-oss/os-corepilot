/*
 * rlimit_demo.c - FR7: demonstrate setrlimit() on three resources.
 *
 *   nofile : RLIMIT_NOFILE lowered  -> open() fails with EMFILE
 *   mem    : RLIMIT_AS lowered      -> malloc() fails with ENOMEM
 *   cpu    : RLIMIT_CPU lowered     -> kernel sends SIGXCPU and the process dies
 *
 * Every test runs in its own forked child so the parent keeps its limits.
 * The child prints getrlimit() before/after and the observed effect; the
 * parent decodes the wait status and checks it matches the expectation.
 */
#include "rlimit_demo.h"
#include "common.h"
#include "logger.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOFILE_LIMIT 16
#define MEM_LIMIT    (256UL * 1024 * 1024)
#define MEM_CHUNK    (16UL * 1024 * 1024)

static const char *lim_str(rlim_t v, char *buf, size_t n)
{
    if (v == RLIM_INFINITY)
        snprintf(buf, n, "unlimited");
    else
        snprintf(buf, n, "%lu", (unsigned long)v);
    return buf;
}

static void show_limit(const char *label, int res)
{
    struct rlimit rl;
    char a[32], b[32];
    if (getrlimit(res, &rl) == 0)
        printf("  [child pid=%d] %-14s soft=%s hard=%s\n", (int)getpid(), label,
               lim_str(rl.rlim_cur, a, sizeof a), lim_str(rl.rlim_max, b, sizeof b));
    fflush(stdout);
}

/* Lower the soft limit (hard limit untouched). Returns 0 on success. */
static int lower(int res, rlim_t soft)
{
    struct rlimit rl;
    if (getrlimit(res, &rl) != 0)
        return -1;
    if (rl.rlim_max != RLIM_INFINITY && soft > rl.rlim_max)
        soft = rl.rlim_max;
    rl.rlim_cur = soft;
    return setrlimit(res, &rl);
}

/* ---- child bodies: return the exit status (0 = effect observed) ---- */

static int child_nofile(void)
{
    int fds[1024], n = 0;
    show_limit("NOFILE before", RLIMIT_NOFILE);
    if (lower(RLIMIT_NOFILE, NOFILE_LIMIT) != 0) {
        printf("  [child] setrlimit failed: %s\n", strerror(errno));
        return 1;
    }
    show_limit("NOFILE after", RLIMIT_NOFILE);
    for (;;) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd < 0)
            break;
        if (n < 1024)
            fds[n++] = fd;
    }
    int e = errno;
    printf("  [child] opened %d extra descriptors, then open() failed: errno=%d (%s)\n",
           n, e, strerror(e));
    printf("  [child] effect: limit %d - 3 standard streams = %d usable descriptors\n",
           NOFILE_LIMIT, NOFILE_LIMIT - 3);
    while (n > 0)
        close(fds[--n]);
    return e == EMFILE ? 0 : 1;
}

static int child_mem(void)
{
    int chunks = 0;
    void *keep[64];
    show_limit("AS before", RLIMIT_AS);
    if (lower(RLIMIT_AS, MEM_LIMIT) != 0) {
        printf("  [child] setrlimit failed: %s\n", strerror(errno));
        return 1;
    }
    show_limit("AS after", RLIMIT_AS);
    for (;;) {
        void *p = malloc(MEM_CHUNK);
        if (!p)
            break;
        if (chunks < 64)
            keep[chunks] = p;
        chunks++;
        if (chunks >= 64)
            break;
    }
    int e = errno;
    printf("  [child] allocated %d x %lu MiB, then malloc() returned NULL: errno=%d (%s)\n",
           chunks, MEM_CHUNK >> 20, e, strerror(e));
    printf("  [child] effect: address-space limit %lu MiB caps total allocations\n", MEM_LIMIT >> 20);
    for (int i = 0; i < chunks && i < 64; i++)
        free(keep[i]);
    return (chunks < 64 && e == ENOMEM) ? 0 : 1;
}

static int child_cpu(void)
{
    struct rlimit no_core = { 0, 0 };
    setrlimit(RLIMIT_CORE, &no_core);
    show_limit("CPU before", RLIMIT_CPU);
    struct rlimit rl;
    getrlimit(RLIMIT_CPU, &rl);
    rl.rlim_cur = 1;                              /* 1 second of CPU time */
    if (rl.rlim_max != RLIM_INFINITY && rl.rlim_max < 1)
        rl.rlim_max = 1;
    if (setrlimit(RLIMIT_CPU, &rl) != 0) {
        printf("  [child] setrlimit failed: %s\n", strerror(errno));
        return 1;
    }
    show_limit("CPU after", RLIMIT_CPU);
    printf("  [child] spinning in an endless loop; kernel should send SIGXCPU after 1 s of CPU time\n");
    fflush(stdout);
    volatile unsigned long x = 0;
    for (;;)
        x++;
    return 1;                                     /* not reached */
}

/* ---- parent side ---- */

typedef struct {
    const char *name, *title;
    int (*body)(void);
    int expect_signal;                            /* 0 = expect exit status 0 */
} Test;

static const Test TESTS[] = {
    { "nofile", "RLIMIT_NOFILE (open files)",   child_nofile, 0 },
    { "mem",    "RLIMIT_AS (address space)",    child_mem,    0 },
    { "cpu",    "RLIMIT_CPU (CPU seconds)",     child_cpu,    SIGXCPU },
};
#define NTESTS 3

static int run_one(const Test *t)
{
    print_section(t->title);
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        print_error("fork: %s", strerror(errno));
        return 0;
    }
    if (pid == 0) {
        int rc = t->body();
        fflush(stdout);
        _exit(rc);
    }
    int st = 0;
    while (waitpid(pid, &st, 0) < 0)
        if (errno != EINTR) {
            print_error("waitpid: %s", strerror(errno));
            return 0;
        }
    int ok;
    if (t->expect_signal) {
        ok = WIFSIGNALED(st) && WTERMSIG(st) == t->expect_signal;
        printf("  [parent] child %d %s (%s)\n", (int)pid,
               WIFSIGNALED(st) ? "was terminated by a signal" : "ended differently",
               WIFSIGNALED(st) ? strsignal(WTERMSIG(st)) : "unexpected");
    } else {
        ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
        printf("  [parent] child %d exited with status %d\n", (int)pid,
               WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    }
    printf("  [parent] expected effect observed: %s\n", ok ? "YES" : "NO");
    log_event("RLIMIT", "%s: %s", t->name, ok ? "effect observed" : "FAILED");
    return ok;
}

int rlimit_scenario(const char *mode, int verbose)
{
    (void)verbose;
    int ran = 0, ok = 1, all = !strcmp(mode, "all");
    for (int i = 0; i < NTESTS; i++)
        if (all || !strcmp(mode, TESTS[i].name)) {
            ok &= run_one(&TESTS[i]);
            ran++;
        }
    if (!ran) {
        print_error("unknown rlimit mode '%s'", mode);
        return 0;
    }
    printf("\nResource-limit result: %s (parent's own limits were never changed)\n",
           ok ? "PASS" : "FAIL");
    return ok;
}

int run_rlimit_cmd(int argc, char **argv)
{
    const char *mode = "all";
    if (argc > 1 || (argc == 1 && strcmp(argv[0], "nofile") && strcmp(argv[0], "mem") &&
                     strcmp(argv[0], "cpu") && strcmp(argv[0], "all"))) {
        fprintf(stderr, "usage: campuscompute rlimit [nofile|mem|cpu|all]\n"
                        "  nofile: RLIMIT_NOFILE -> open() fails (EMFILE)\n"
                        "  mem   : RLIMIT_AS     -> malloc() fails (ENOMEM)\n"
                        "  cpu   : RLIMIT_CPU    -> process killed by SIGXCPU (~1 s)\n");
        return EXIT_USAGE;
    }
    if (argc == 1)
        mode = argv[0];
    print_banner("Resource limits (setrlimit)");
    return rlimit_scenario(mode, 1) ? EXIT_OK : EXIT_FAIL;
}
