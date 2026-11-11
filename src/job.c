/*
 * job.c - FR1: job file parsing, validation, lifecycle helpers, generator.
 *
 * File format (one job per line, '#' starts a comment):
 *     <id> <name> <arrival> <burst> <priority>
 */
#include "job.h"

#include <sys/stat.h>

#define ID_MAX       9999
#define ARRIVAL_MAX  1000
#define BURST_MAX    100
#define PRIO_MAX     99

const char *job_status_str(JobStatus s)
{
    switch (s) {
    case JOB_NEW:     return "NEW";
    case JOB_READY:   return "READY";
    case JOB_RUNNING: return "RUNNING";
    case JOB_DONE:    return "DONE";
    }
    return "?";
}

void job_reset_runtime(Job *j)
{
    j->status      = JOB_NEW;
    j->remaining   = j->burst;
    j->first_start = -1;
    j->completion = j->turnaround = j->waiting = j->response = 0;
}

/* Report a parse problem with its line number. */
#define FAIL(...)                                          \
    do {                                                   \
        int _n = snprintf(err, errlen, "%s:%d: ", path, lineno); \
        if (_n > 0 && (size_t)_n < errlen)                 \
            snprintf(err + _n, errlen - (size_t)_n, __VA_ARGS__); \
        fclose(f);                                         \
        return -1;                                         \
    } while (0)

int job_load(const char *path, JobSet *js, char *err, size_t errlen)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, errlen, "cannot open '%s': %s", path, strerror(errno));
        return -1;
    }
    memset(js, 0, sizeof *js);

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (!strchr(line, '\n') && !feof(f))
            FAIL("line too long");

        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';

        char *tok[8], *save = NULL;
        int nt = 0;
        for (char *p = strtok_r(line, " \t\r\n", &save); p;
             p = strtok_r(NULL, " \t\r\n", &save)) {
            if (nt < 8)
                tok[nt] = p;
            nt++;
        }
        if (nt == 0)
            continue;                       /* blank / comment-only line */
        if (nt != 5)
            FAIL("expected 5 fields (id name arrival burst priority), found %d", nt);
        if (js->count >= MAX_JOBS)
            FAIL("too many jobs (limit %d)", MAX_JOBS);

        Job j;
        memset(&j, 0, sizeof j);
        if (cc_parse_int(tok[0], 1, ID_MAX, &j.id))
            FAIL("job id '%s' must be an integer 1..%d", tok[0], ID_MAX);
        if (strlen(tok[1]) >= sizeof j.name)
            FAIL("job name '%s' is too long (max %zu chars)", tok[1], sizeof j.name - 1);
        strcpy(j.name, tok[1]);
        if (cc_parse_int(tok[2], 0, ARRIVAL_MAX, &j.arrival))
            FAIL("arrival '%s' must be an integer 0..%d", tok[2], ARRIVAL_MAX);
        if (cc_parse_int(tok[3], 1, BURST_MAX, &j.burst))
            FAIL("burst '%s' must be an integer 1..%d", tok[3], BURST_MAX);
        if (cc_parse_int(tok[4], 1, PRIO_MAX, &j.priority))
            FAIL("priority '%s' must be an integer 1..%d", tok[4], PRIO_MAX);

        for (int i = 0; i < js->count; i++)
            if (js->jobs[i].id == j.id)
                FAIL("duplicate job id %d", j.id);

        job_reset_runtime(&j);
        js->jobs[js->count++] = j;
    }
    fclose(f);

    if (js->count == 0) {
        snprintf(err, errlen, "%s: no jobs found", path);
        return -1;
    }
    return 0;
}

int jobset_load(const char *path, JobSet *js)
{
    char err[256];
    if (job_load(path, js, err, sizeof err)) {
        print_error("%s", err);
        return -1;
    }
    return 0;
}

void job_print_input(const JobSet *js, const char *path)
{
    printf("Loaded %d jobs from %s\n", js->count, path);
    printf("  %-4s %-14s %7s %6s %5s  %s\n", "ID", "Name", "Arrival", "Burst", "Prio", "Status");
    for (int i = 0; i < js->count; i++) {
        const Job *j = &js->jobs[i];
        printf("  %-4d %-14s %7d %6d %5d  %s\n", j->id, j->name, j->arrival,
               j->burst, j->priority, job_status_str(j->status));
    }
}

/* ------------------------------------------------------------------ */
/* Job generator: reproducible workloads from a seed.                  */
/* ------------------------------------------------------------------ */

static const char *const NAMES[] = {
    "matlab-sim", "cfd-solver", "ml-train", "latex-build", "gene-align",
    "render", "monte-carlo", "compile", "fft-bench", "db-index"
};

int run_gen(int argc, char **argv)
{
    int count, seed;
    if (argc != 3 || cc_parse_int(argv[0], 1, MAX_JOBS, &count) ||
        cc_parse_int(argv[1], 0, 1000000, &seed)) {
        fprintf(stderr, "usage: campuscompute gen <count 1..%d> <seed> <outfile>\n", MAX_JOBS);
        return EXIT_USAGE;
    }
    struct stat st;
    if (stat(argv[2], &st) == 0) {
        fprintf(stderr, "error: '%s' already exists; refusing to overwrite it\n", argv[2]);
        return EXIT_FAIL;
    }
    FILE *f = fopen(argv[2], "w");
    if (!f) {
        fprintf(stderr, "error: cannot create '%s': %s\n", argv[2], strerror(errno));
        return EXIT_FAIL;
    }
    srand((unsigned)seed);
    fprintf(f, "# generated by `campuscompute gen %d %d`\n", count, seed);
    fprintf(f, "# id name arrival burst priority   (priority 1 = most urgent)\n");
    int t = 0;
    for (int i = 1; i <= count; i++) {
        t += rand() % 4;                         /* arrivals never go backwards */
        fprintf(f, "%d %s-%d %d %d %d\n", i, NAMES[rand() % 10], i, t,
                1 + rand() % 12, 1 + rand() % 5);
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "error: writing '%s' failed: %s\n", argv[2], strerror(errno));
        return EXIT_FAIL;
    }
    printf("Wrote %d jobs (seed %d) to %s\n", count, seed, argv[2]);
    return EXIT_OK;
}
