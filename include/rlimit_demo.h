/* rlimit_demo.h - FR7: process resource limits with setrlimit(). */
#ifndef CAMPUS_RLIMIT_DEMO_H
#define CAMPUS_RLIMIT_DEMO_H

/* mode: "nofile", "mem", "cpu" or "all". Each test runs in a forked child so
 * the limits never affect the parent. Returns 1 when every observed effect
 * matched what the limit should cause. */
int rlimit_scenario(const char *mode, int verbose);
int run_rlimit_cmd(int argc, char **argv);

#endif
