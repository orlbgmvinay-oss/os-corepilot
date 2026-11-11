/*
 * common.c - small helpers: IPC names, strict parsing, thread-safe log append.
 */
#include "common.h"

#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int cc_parse_int(const char *s, long lo, long hi, int *out)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < lo || v > hi)
        return -1;
    *out = (int)v;
    return 0;
}

int cc_opt_int(int argc, char **argv, int *i, const char *name,
               long lo, long hi, int *out)
{
    if (strcmp(argv[*i], name) != 0)
        return 0;
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: option %s needs a value\n", name);
        return -1;
    }
    if (cc_parse_int(argv[*i + 1], lo, hi, out) != 0) {
        fprintf(stderr, "error: option %s expects an integer in [%ld, %ld], got '%s'\n",
                name, lo, hi, argv[*i + 1]);
        return -1;
    }
    (*i)++;
    return 1;
}

void app_log(const char *level, const char *fmt, ...)
{
    char msg[384], line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    struct timespec ts;
    struct tm tmv;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmv);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tmv);

    int len = snprintf(line, sizeof line, "[%s.%03ld] [pid %5d] [%-7s] %s\n",
                       stamp, ts.tv_nsec / 1000000, (int)getpid(), level, msg);
    if (len < 0)
        return;
    if (len >= (int)sizeof line)
        len = (int)sizeof line - 1;

    /* The logs/ directory is ours; create it on first use. */
    if (mkdir(LOG_DIR, 0755) != 0 && errno != EEXIST)
        return;
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return;
    /* One write() per line on an O_APPEND fd keeps lines intact even when
     * several threads/processes log at once. */
    if (write(fd, line, (size_t)len) < 0) {
        /* best effort: logging must never abort the program */
    }
    close(fd);
}

long cc_count_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    long n = 0;
    int c;
    while ((c = fgetc(f)) != EOF)
        if (c == '\n')
            n++;
    fclose(f);
    return n;
}

int cc_thread_count(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    if (!f)
        return -1;
    char line[256];
    int threads = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "Threads: %d", &threads) == 1)
            break;
    fclose(f);
    return threads;
}

void print_banner(const char *title)
{
    printf("\n==================== %s ====================\n", title);
}

void print_section(const char *title)
{
    printf("\n--- %s ---\n", title);
}

void print_error(const char *fmt, ...)
{
    va_list ap;
    fflush(stdout);
    fprintf(stderr, "error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

int parse_int_arg(const char *s, int lo, int hi, int *out)
{
    return cc_parse_int(s, lo, hi, out);
}

void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
}

void cc_print_lines(const char *path, long first, int max)
{
    FILE *f = fopen(path, "r");
    char line[600];
    long n = 0;
    int shown = 0;
    if (!f)
        return;
    while (shown < max && fgets(line, sizeof line, f)) {
        if (n++ < first)
            continue;
        printf("  %s", line);
        shown++;
    }
    fclose(f);
}
