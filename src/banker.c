/*
 * banker.c - FR5: Banker's safety algorithm (+ resource-request check).
 *
 * Input file (keywords, '#' comments, any whitespace):
 *     processes N
 *     resources M
 *     names  R1 R2 ... RM          (optional labels)
 *     available a1 ... aM
 *     max         N rows of M numbers
 *     allocation  N rows of M numbers
 *     request <pid> v1 ... vM      (optional, repeatable)
 *
 * Need[i][j] = Max[i][j] - Allocation[i][j].
 * Safety algorithm: Work = Available; repeatedly pick the lowest-numbered
 * unfinished process whose Need <= Work, let it finish (Work += Allocation).
 * If every process finishes the state is SAFE and the order is a safe sequence.
 */
#include "banker.h"
#include "common.h"

#include <strings.h>

#define MAX_REQ 16
#define MAX_FILE (64 * 1024)

typedef struct {
    int n, m;
    char names[MAX_RES][12];
    int avail[MAX_RES];
    int max[MAX_PROCS][MAX_RES];
    int alloc[MAX_PROCS][MAX_RES];
    int need[MAX_PROCS][MAX_RES];
    int nreq;
    struct { int pid; int vec[MAX_RES]; } req[MAX_REQ];
} Bank;

/* ---------------------------------------------------------------- */
/* Parsing                                                           */
/* ---------------------------------------------------------------- */

static char *slurp(const char *path, char *err, size_t errlen)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, errlen, "cannot open '%s': %s", path, strerror(errno));
        return NULL;
    }
    char *buf = malloc(MAX_FILE + 1);
    if (!buf) {
        fclose(f);
        snprintf(err, errlen, "out of memory");
        return NULL;
    }
    size_t n = fread(buf, 1, MAX_FILE + 1, f);
    fclose(f);
    if (n > MAX_FILE) {
        free(buf);
        snprintf(err, errlen, "'%s' is too large", path);
        return NULL;
    }
    buf[n] = '\0';
    /* blank out comments so the tokenizer never sees them */
    for (char *p = buf; *p; p++)
        if (*p == '#')
            while (*p && *p != '\n')
                *p++ = ' ';
    return buf;
}

#define NEXT() strtok_r(NULL, " \t\r\n", &save)
#define BAD(...) do { snprintf(err, errlen, __VA_ARGS__); free(buf); return -1; } while (0)

static int read_vec(char **save, int *v, int m, int lo, const char *what,
                    char *err, size_t errlen)
{
    for (int j = 0; j < m; j++) {
        char *t = strtok_r(NULL, " \t\r\n", save);
        if (!t || cc_parse_int(t, lo, 1000000, &v[j])) {
            snprintf(err, errlen, "%s: expected %d non-negative integers (bad value '%s' at position %d)",
                     what, m, t ? t : "<end of file>", j + 1);
            return -1;
        }
    }
    return 0;
}

static int bank_load(const char *path, Bank *b, char *err, size_t errlen)
{
    char *buf = slurp(path, err, errlen), *save = NULL;
    if (!buf)
        return -1;
    memset(b, 0, sizeof *b);
    int have_av = 0, have_max = 0, have_al = 0;

    for (char *t = strtok_r(buf, " \t\r\n", &save); t; t = NEXT()) {
        char *v;
        if (!strcasecmp(t, "processes")) {
            if (!(v = NEXT()) || cc_parse_int(v, 1, MAX_PROCS, &b->n))
                BAD("'processes' needs an integer 1..%d", MAX_PROCS);
        } else if (!strcasecmp(t, "resources")) {
            if (!(v = NEXT()) || cc_parse_int(v, 1, MAX_RES, &b->m))
                BAD("'resources' needs an integer 1..%d", MAX_RES);
        } else {
            if (b->n == 0 || b->m == 0)
                BAD("'processes' and 'resources' must be declared before '%s'", t);
            if (!strcasecmp(t, "names")) {
                for (int j = 0; j < b->m; j++) {
                    if (!(v = NEXT()) || strlen(v) >= sizeof b->names[j])
                        BAD("'names' needs %d short labels", b->m);
                    strcpy(b->names[j], v);
                }
            } else if (!strcasecmp(t, "available")) {
                if (read_vec(&save, b->avail, b->m, 0, "available", err, errlen)) { free(buf); return -1; }
                have_av = 1;
            } else if (!strcasecmp(t, "max") || !strcasecmp(t, "allocation")) {
                int is_max = !strcasecmp(t, "max");
                for (int i = 0; i < b->n; i++)
                    if (read_vec(&save, is_max ? b->max[i] : b->alloc[i], b->m, 0,
                                 is_max ? "max" : "allocation", err, errlen)) {
                        free(buf);
                        return -1;
                    }
                if (is_max) have_max = 1; else have_al = 1;
            } else if (!strcasecmp(t, "request")) {
                if (b->nreq >= MAX_REQ)
                    BAD("too many 'request' lines (limit %d)", MAX_REQ);
                if (!(v = NEXT()) || cc_parse_int(v, 0, b->n - 1, &b->req[b->nreq].pid))
                    BAD("'request' needs a process number 0..%d", b->n - 1);
                if (read_vec(&save, b->req[b->nreq].vec, b->m, 0, "request", err, errlen)) {
                    free(buf);
                    return -1;
                }
                b->nreq++;
            } else {
                BAD("unknown keyword '%s'", t);
            }
        }
    }
    free(buf);

    if (!b->n || !b->m) { snprintf(err, errlen, "missing 'processes'/'resources'"); return -1; }
    if (!have_av)  { snprintf(err, errlen, "missing 'available' section"); return -1; }
    if (!have_max) { snprintf(err, errlen, "missing 'max' section"); return -1; }
    if (!have_al)  { snprintf(err, errlen, "missing 'allocation' section"); return -1; }

    for (int j = 0; j < b->m; j++)
        if (!b->names[j][0])
            snprintf(b->names[j], sizeof b->names[j], "R%d", j);

    /* Need = Max - Allocation; allocation may never exceed the declared maximum */
    for (int i = 0; i < b->n; i++)
        for (int j = 0; j < b->m; j++) {
            b->need[i][j] = b->max[i][j] - b->alloc[i][j];
            if (b->need[i][j] < 0) {
                snprintf(err, errlen, "P%d holds more %s (%d) than its maximum claim (%d)",
                         i, b->names[j], b->alloc[i][j], b->max[i][j]);
                return -1;
            }
        }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Printing                                                          */
/* ---------------------------------------------------------------- */

static void vec_str(char *out, size_t n, const int *v, int m)
{
    size_t l = (size_t)snprintf(out, n, "[");
    for (int j = 0; j < m && l < n; j++)
        l += (size_t)snprintf(out + l, n - l, "%s%d", j ? " " : "", v[j]);
    if (l < n)
        snprintf(out + l, n - l, "]");
}

static void print_matrix(const char *title, const Bank *b, int mat[][MAX_RES])
{
    printf("%s\n      ", title);
    for (int j = 0; j < b->m; j++)
        printf("%6s", b->names[j]);
    printf("\n");
    for (int i = 0; i < b->n; i++) {
        printf("  P%-3d", i);
        for (int j = 0; j < b->m; j++)
            printf("%6d", mat[i][j]);
        printf("\n");
    }
}

static void print_state(const Bank *b)
{
    int total[MAX_RES];
    char s[64];
    printf("Processes: %d   Resource types: %d\n\n", b->n, b->m);
    print_matrix("Allocation:", b, (int (*)[MAX_RES])b->alloc);
    printf("\n");
    print_matrix("Maximum:", b, (int (*)[MAX_RES])b->max);
    printf("\n");
    print_matrix("Need = Maximum - Allocation:", b, (int (*)[MAX_RES])b->need);
    vec_str(s, sizeof s, b->avail, b->m);
    printf("\nAvailable: %s\n", s);
    for (int j = 0; j < b->m; j++) {
        total[j] = b->avail[j];
        for (int i = 0; i < b->n; i++)
            total[j] += b->alloc[i][j];
    }
    vec_str(s, sizeof s, total, b->m);
    printf("Total system resources (Available + sum of Allocation): %s\n", s);
}

/* ---------------------------------------------------------------- */
/* Safety algorithm                                                  */
/* ---------------------------------------------------------------- */

static int vec_le(const int *a, const int *b, int m)
{
    for (int j = 0; j < m; j++)
        if (a[j] > b[j])
            return 0;
    return 1;
}

/* Returns 1 if safe (seq[] filled with the safe sequence), 0 if unsafe. */
static int is_safe(const Bank *b, int *seq, int verbose)
{
    int work[MAX_RES], fin[MAX_PROCS] = {0}, cnt = 0;
    char s1[64], s2[64], s3[64];
    memcpy(work, b->avail, sizeof work);

    while (cnt < b->n) {
        int pick = -1;
        for (int i = 0; i < b->n && pick < 0; i++)
            if (!fin[i] && vec_le(b->need[i], work, b->m))
                pick = i;
        if (pick < 0)
            break;
        vec_str(s1, sizeof s1, b->need[pick], b->m);
        vec_str(s2, sizeof s2, work, b->m);
        for (int j = 0; j < b->m; j++)
            work[j] += b->alloc[pick][j];
        vec_str(s3, sizeof s3, work, b->m);
        if (verbose)
            printf("  step %d: P%d  Need %s <= Work %s  -> finishes, releases its allocation, Work = %s\n",
                   cnt + 1, pick, s1, s2, s3);
        fin[pick] = 1;
        seq[cnt++] = pick;
    }
    if (cnt == b->n)
        return 1;

    if (verbose) {
        vec_str(s2, sizeof s2, work, b->m);
        printf("  no unfinished process can proceed with Work = %s:\n", s2);
        for (int i = 0; i < b->n; i++) {
            if (fin[i])
                continue;
            vec_str(s1, sizeof s1, b->need[i], b->m);
            printf("    P%d needs %s  (short of:", i, s1);
            for (int j = 0; j < b->m; j++)
                if (b->need[i][j] > work[j])
                    printf(" %s by %d", b->names[j], b->need[i][j] - work[j]);
            printf(")\n");
        }
    }
    return 0;
}

static void seq_str(char *out, size_t n, const int *seq, int cnt)
{
    size_t l = 0;
    out[0] = '\0';
    for (int i = 0; i < cnt && l < n; i++)
        l += (size_t)snprintf(out + l, n - l, "%sP%d", i ? " -> " : "", seq[i]);
}

/* ---------------------------------------------------------------- */
/* Resource-request check (Banker's resource-request algorithm)       */
/* ---------------------------------------------------------------- */

static void check_requests(Bank *b)
{
    for (int r = 0; r < b->nreq; r++) {
        int p = b->req[r].pid;
        const int *v = b->req[r].vec;
        char vs[64], seq[160];
        int order[MAX_PROCS];
        vec_str(vs, sizeof vs, v, b->m);
        printf("\nRequest %d: P%d asks for %s\n", r + 1, p, vs);

        if (!vec_le(v, b->need[p], b->m)) {
            printf("  DENIED: request exceeds P%d's declared maximum claim (Need).\n", p);
            app_log("BANKER", "request P%d %s denied: exceeds Need", p, vs);
            continue;
        }
        if (!vec_le(v, b->avail, b->m)) {
            printf("  DENIED (must wait): not enough Available resources right now.\n");
            app_log("BANKER", "request P%d %s denied: not available", p, vs);
            continue;
        }
        /* pretend to grant, test safety, roll back if unsafe */
        Bank trial = *b;
        for (int j = 0; j < b->m; j++) {
            trial.avail[j] -= v[j];
            trial.alloc[p][j] += v[j];
            trial.need[p][j] -= v[j];
        }
        if (is_safe(&trial, order, 0)) {
            seq_str(seq, sizeof seq, order, b->n);
            printf("  GRANTED: state after granting is SAFE. Safe sequence: %s\n", seq);
            app_log("BANKER", "request P%d %s granted", p, vs);
            *b = trial;
        } else {
            printf("  DENIED: granting would leave the system in an UNSAFE state (rolled back).\n");
            app_log("BANKER", "request P%d %s denied: unsafe", p, vs);
        }
    }
}

/* ---------------------------------------------------------------- */
/* Entry point                                                       */
/* ---------------------------------------------------------------- */

int run_banker(int argc, char **argv)
{
    if (argc != 1) {
        fprintf(stderr, "usage: campuscompute banker <statefile>\n");
        return EXIT_USAGE;
    }
    Bank b;
    char err[256];
    if (bank_load(argv[0], &b, err, sizeof err)) {
        fprintf(stderr, "error: %s: %s\n", argv[0], err);
        return EXIT_USAGE;
    }
    print_banner("BANKER'S ALGORITHM");
    printf("Input file: %s\n", argv[0]);
    print_state(&b);

    printf("\nSafety algorithm (scan P0..P%d, pick the first unfinished process with Need <= Work):\n",
           b.n - 1);
    int seq[MAX_PROCS];
    char s[160];
    if (is_safe(&b, seq, 1)) {
        seq_str(s, sizeof s, seq, b.n);
        printf("\nRESULT: SAFE STATE.  Safe sequence: <%s>\n", s);
        app_log("BANKER", "%s: SAFE, sequence %s", argv[0], s);
    } else {
        printf("\nRESULT: UNSAFE STATE.  No safe sequence exists - deadlock is possible.\n");
        app_log("BANKER", "%s: UNSAFE", argv[0]);
    }
    if (b.nreq)
        check_requests(&b);
    return EXIT_OK;
}
