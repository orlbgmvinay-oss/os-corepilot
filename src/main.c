/*
 * main.c - command-line dispatch only; each feature lives in its own module
 * and is independently runnable (FR8).
 */
#include "banker.h"
#include "common.h"
#include "demo.h"
#include "ipc.h"
#include "job.h"
#include "logger.h"
#include "rlimit_demo.h"
#include "scheduler.h"
#include "worker.h"

static void usage(FILE *out)
{
    fprintf(out,
"%s %s - Intelligent Job Scheduling and Resource Management\n\n"
"Usage: campuscompute <command> [arguments]\n\n"
"Commands:\n"
"  help                                   show this text\n"
"  gen <count> <seed> <outfile>           generate a reproducible random job file\n"
"  sched fcfs|rr|priority|compare <jobfile> [--quantum N] [--aging N]\n"
"                                         CPU scheduling (FR1, FR2)\n"
"  logger [--producers P] [--consumers C] [--items N] [--capacity K]\n"
"                                         pthread producer-consumer logger (FR3)\n"
"  ipc [jobfile] [--policy P] [--quantum N]\n"
"                                         shared memory + named semaphores (FR4)\n"
"  banker <statefile>                     Banker's safety algorithm (FR5)\n"
"  worker [normal|fail|crash|hang|mix] [--timeout S]\n"
"                                         fork/waitpid failure detection (FR6)\n"
"  rlimit [nofile|mem|cpu|all]            setrlimit demonstrations (FR7)\n"
"  demo [jobfile]                         run everything and print a summary\n"
"  cleanup [--logs]                       remove this program's IPC objects (and log)\n\n"
"Examples:\n"
"  ./campuscompute sched rr tests/jobs_basic.txt --quantum 3\n"
"  ./campuscompute banker tests/banker_unsafe.txt\n"
"  ./campuscompute worker mix\n", APP_NAME, APP_VERSION);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);       /* keep parent/child output in order */
    if (argc < 2) {
        fprintf(stderr, "error: no command given\n\n");
        usage(stderr);
        return EXIT_USAGE;
    }
    const char *cmd = argv[1];
    int n = argc - 2;
    char **a = argv + 2;

    if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) { usage(stdout); return EXIT_OK; }
    if (!strcmp(cmd, "--version"))  { printf("%s %s\n", APP_NAME, APP_VERSION); return EXIT_OK; }
    if (!strcmp(cmd, "gen"))        return run_gen(n, a);
    if (!strcmp(cmd, "sched"))      return run_scheduler(n, a);
    if (!strcmp(cmd, "logger"))     return run_logger_cmd(n, a);
    if (!strcmp(cmd, "ipc"))        return run_ipc_cmd(n, a);
    if (!strcmp(cmd, "banker"))     return run_banker(n, a);
    if (!strcmp(cmd, "worker"))     return run_worker_cmd(n, a);
    if (!strcmp(cmd, "rlimit"))     return run_rlimit_cmd(n, a);
    if (!strcmp(cmd, "demo"))       return run_demo(n, a);
    if (!strcmp(cmd, "cleanup"))    return run_cleanup(n, a);

    fprintf(stderr, "error: unknown command '%s'\n\n", cmd);
    usage(stderr);
    return EXIT_USAGE;
}
