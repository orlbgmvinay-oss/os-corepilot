/*
 * common.h - shared constants and small helpers used by every module.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define APP_NAME    "CampusCompute"
#define APP_VERSION "1.0"

/* Limits shared by the modules (all inputs are validated against these). */
#define MAX_JOBS  64
#define MAX_PROCS 16
#define MAX_RES   8

/* The one log file written by this program (and the directory we create). */
#define LOG_DIR  "logs"
#define LOG_PATH "logs/campuscompute.log"

/* Process exit codes. */
#define EXIT_OK    0
#define EXIT_FAIL  1   /* runtime failure                  */
#define EXIT_USAGE 2   /* bad command / bad option / input */

/* POSIX IPC object names created (and only ever removed) by this program. */
#define SHM_NAME     "/campuscompute_shm"
#define SEM_REQ_NAME "/campuscompute_req"
#define SEM_ACK_NAME "/campuscompute_ack"

#define MAX_NAME_LEN     24                      /* job name buffer size */
#define DEFAULT_JOBS_FILE "tests/jobs_basic.txt"
#define DEFAULT_QUANTUM  3
#define MAX_QUANTUM      50

/* Strict integer parse: whole string must be a number within [lo, hi].
 * Returns 0 on success, -1 on failure. */
int cc_parse_int(const char *s, long lo, long hi, int *out);

/* Option helper: if argv[*i] equals `name`, consume its value into *out.
 * Returns 1 = matched, 0 = not this option, -1 = error (already reported). */
int cc_opt_int(int argc, char **argv, int *i, const char *name,
               long lo, long hi, int *out);

/* Append one timestamped line to LOG_PATH. Safe to call from threads and
 * from forked children (single O_APPEND write per line). */
void app_log(const char *level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Number of lines currently in a file (0 if it does not exist). */
long cc_count_lines(const char *path);

/* Number of threads in this process, read from /proc/self/status (-1 on error). */
int cc_thread_count(void);

/* Console formatting and small utilities. */
void print_banner(const char *title);            /* major heading   */
void print_section(const char *title);           /* sub heading     */
void print_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int  parse_int_arg(const char *s, int lo, int hi, int *out);
void sleep_ms(int ms);

/* Print up to `max` lines of a file starting at 0-based line `first`. */
void cc_print_lines(const char *path, long first, int max);

#endif /* COMMON_H */
