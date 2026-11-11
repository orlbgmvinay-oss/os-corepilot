/*
 * demo.c - integrated run of every module plus the cleanup commands.
 */
#include "demo.h"
#include "banker.h"
#include "common.h"
#include "ipc.h"
#include "logger.h"
#include "rlimit_demo.h"
#include "scheduler.h"
#include "worker.h"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#define NSTAGES 8

/* Post-run checks: no child left, one thread, no program-created IPC. */
static int verify_clean(void)
{
    int ok = 1;
    errno = 0;
    pid_t r = waitpid(-1, NULL, WNOHANG);
    int no_children = (r == -1 && errno == ECHILD);
    int threads = cc_thread_count();
    int ipc_left = ipc_objects_present(0);

    printf("  Unreaped child processes : %s\n", no_children ? "none (waitpid -> ECHILD)" : "SOME REMAIN");
    printf("  Threads in this process  : %d (only the main thread)\n", threads);
    printf("  Program-created IPC left : %d\n", ipc_left);
    ok = no_children && threads == 1 && ipc_left == 0;
    return ok;
}

int run_demo(int argc, char **argv)
{
    const char *jobs = DEFAULT_JOBS_FILE;
    if (argc > 1) {
        fprintf(stderr, "usage: campuscompute demo [jobfile]\n");
        return EXIT_USAGE;
    }
    if (argc == 1)
        jobs = argv[0];

    const char *names[NSTAGES] = {
        "Scheduling comparison (FR1/FR2)", "Producer-consumer logger (FR3)",
        "Shared memory + semaphores (FR4)", "Banker's safe state (FR5)",
        "Banker's unsafe state (FR5)", "Worker failure detection (FR6)",
        "Resource limits (FR7)", "Cleanup verification (FR7)"
    };
    int ok[NSTAGES] = {0};
    long log_before = cc_count_lines(LOG_PATH);

    print_banner("CAMPUSCOMPUTE INTEGRATED DEMONSTRATION");
    printf("Job file: %s\n", jobs);
    app_log("DEMO", "integrated demonstration started");

    { char *a[] = { "compare", (char *)jobs, "--quantum", "3", "--aging", "2" };
      ok[0] = run_scheduler(6, a) == EXIT_OK; }

    print_banner("Concurrent logger");
    { LoggerConfig cfg = { 3, 2, 4, 4 }; ok[1] = logger_demo(&cfg, 1); }

    print_banner("Shared-memory IPC");
    ok[2] = ipc_demo(jobs, POLICY_RR, DEFAULT_QUANTUM, 1);

    { char *a[] = { "tests/banker_safe.txt" };   ok[3] = run_banker(1, a) == EXIT_OK; }
    { char *a[] = { "tests/banker_unsafe.txt" }; ok[4] = run_banker(1, a) == EXIT_OK; }

    print_banner("Worker processes");
    ok[5] = worker_scenario("mix", 1, 1);

    ok[6] = (print_banner("Resource limits"), rlimit_scenario("all", 1));

    print_banner("Cleanup verification");
    ok[7] = verify_clean();

    print_banner("DEMO SUMMARY");
    int pass = 0;
    for (int i = 0; i < NSTAGES; i++) {
        printf("  %d. %-36s %s\n", i + 1, names[i], ok[i] ? "PASS" : "FAIL");
        pass += ok[i];
        app_log("DEMO", "stage %d (%s): %s", i + 1, names[i], ok[i] ? "PASS" : "FAIL");
    }
    long added = cc_count_lines(LOG_PATH) - log_before;
    printf("\n  %d of %d stages passed.  %ld line(s) added to %s\n", pass, NSTAGES, added, LOG_PATH);
    printf("  Overall: %s\n", pass == NSTAGES ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return pass == NSTAGES ? EXIT_OK : EXIT_FAIL;
}

int run_cleanup(int argc, char **argv)
{
    int logs = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--logs")) {
            logs = 1;
        } else {
            fprintf(stderr, "error: unknown option '%s'\nusage: campuscompute cleanup [--logs]\n", argv[i]);
            return EXIT_USAGE;
        }
    }
    print_banner("Cleanup");
    printf("Removing only the IPC objects this program creates:\n");
    int removed = ipc_unlink_all(1);
    if (removed == 0)
        printf("  nothing to remove (no program-created IPC objects existed)\n");
    if (logs) {
        if (unlink(LOG_PATH) == 0) {
            printf("  removed %s\n", LOG_PATH);
            rmdir(LOG_DIR);                      /* only succeeds if empty */
        } else if (errno != ENOENT) {
            print_error("cannot remove %s: %s", LOG_PATH, strerror(errno));
        }
    }
    printf("\nVerification:\n");
    int left = ipc_objects_present(1);
    printf("  Program-created IPC objects remaining: %d\n", left);
    return left == 0 ? EXIT_OK : EXIT_FAIL;
}
