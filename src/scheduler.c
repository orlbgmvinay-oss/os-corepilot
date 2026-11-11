/*
 * scheduler.c - FR2: CPU scheduling simulators and reports.
 *
 * Every simulator works on its own copy of the JobSet and records a
 * Timeline (who held the CPU in each interval).  All metrics are then
 * computed from that timeline, so nothing is hard-coded:
 *
 *   CT  = completion time          TAT = CT - arrival
 *   WT  = TAT - burst              RT  = first CPU time - arrival
 *
 * Tie-breaking rules (stated again in every report):
 *   FCFS     : equal arrival  -> lower job ID first
 *   RR       : a job that arrives at the exact moment a slice ends joins the
 *              ready queue BEFORE the pre-empted job; equal arrivals by lower ID
 *   PRIORITY : non-preemptive, lower number = more urgent; equal priority ->
 *              earlier arrival, then lower job ID.  Optional aging lowers the
 *              effective priority number by 1 for every <aging> time units a
 *              job has waited, which prevents starvation.
 */
#include "scheduler.h"
#include "common.h"
#include "job.h"

#define MAX_SEGS        8192
#define GANTT_PER_ROW   10

typedef struct { int job; int start; int end; } Seg;     /* job = -1 -> idle */
typedef struct { Seg seg[MAX_SEGS]; int n; } Timeline;

typedef struct {
    char   label[40];
    double avg_ct, avg_tat, avg_wt, avg_rt;
    double util;           /* % of [0, makespan] the CPU was busy */
    double throughput;     /* jobs per time unit                  */
    int    ctx;            /* switches between different jobs     */
    int    makespan;
} Metrics;

/* ---------------------------------------------------------------- */
/* Timeline helpers                                                  */
/* ---------------------------------------------------------------- */

/* Append an interval, merging it with the previous one if contiguous and
 * owned by the same job (so RR slices of a lone job show as one block). */
static int add_seg(Timeline *tl, int job, int s, int e)
{
    if (e <= s)
        return 0;
    if (tl->n > 0) {
        Seg *l = &tl->seg[tl->n - 1];
        if (l->job == job && l->end == s) {
            l->end = e;
            return 0;
        }
    }
    if (tl->n >= MAX_SEGS)
        return -1;
    tl->seg[tl->n].job = job;
    tl->seg[tl->n].start = s;
    tl->seg[tl->n].end = e;
    tl->n++;
    return 0;
}

/* ord[] = job indices sorted by (arrival, id). */
static void sort_by_arrival(const JobSet *w, int *ord)
{
    for (int i = 0; i < w->count; i++)
        ord[i] = i;
    for (int i = 1; i < w->count; i++) {
        int k = ord[i], j = i - 1;
        while (j >= 0) {
            const Job *a = &w->jobs[ord[j]], *b = &w->jobs[k];
            if (a->arrival > b->arrival || (a->arrival == b->arrival && a->id > b->id)) {
                ord[j + 1] = ord[j];
                j--;
            } else {
                break;
            }
        }
        ord[j + 1] = k;
    }
}

/* ---------------------------------------------------------------- */
/* Simulators                                                        */
/* ---------------------------------------------------------------- */

static int sim_fcfs(JobSet *w, Timeline *tl)
{
    int ord[MAX_JOBS], t = 0;
    sort_by_arrival(w, ord);
    for (int k = 0; k < w->count; k++) {
        Job *j = &w->jobs[ord[k]];
        if (t < j->arrival) {                          /* CPU idle until it arrives */
            if (add_seg(tl, -1, t, j->arrival))
                return -1;
            t = j->arrival;
        }
        j->status = JOB_RUNNING;
        j->first_start = t;
        if (add_seg(tl, ord[k], t, t + j->burst))
            return -1;
        t += j->burst;
        j->remaining = 0;
        j->completion = t;
        j->status = JOB_DONE;
    }
    return 0;
}

static int sim_rr(JobSet *w, int quantum, Timeline *tl)
{
    int ord[MAX_JOBS], queue[MAX_JOBS + 1];
    int qh = 0, qt = 0, qn = 0;        /* circular ready queue */
    int next = 0, done = 0, t = 0;
    sort_by_arrival(w, ord);

#define ENQUEUE(x)  do { queue[qt] = (x); qt = (qt + 1) % (MAX_JOBS + 1); qn++; } while (0)

    while (done < w->count) {
        /* admit everything that has arrived by now */
        while (next < w->count && w->jobs[ord[next]].arrival <= t) {
            w->jobs[ord[next]].status = JOB_READY;
            ENQUEUE(ord[next]);
            next++;
        }
        if (qn == 0) {                                  /* idle gap */
            int na = w->jobs[ord[next]].arrival;
            if (add_seg(tl, -1, t, na))
                return -1;
            t = na;
            continue;
        }
        int idx = queue[qh];
        qh = (qh + 1) % (MAX_JOBS + 1);
        qn--;
        Job *j = &w->jobs[idx];
        if (j->first_start < 0)
            j->first_start = t;
        j->status = JOB_RUNNING;

        int run = j->remaining < quantum ? j->remaining : quantum;
        if (add_seg(tl, idx, t, t + run))
            return -1;
        t += run;
        j->remaining -= run;

        /* arrivals during the slice (incl. exactly at its end) queue first */
        while (next < w->count && w->jobs[ord[next]].arrival <= t) {
            w->jobs[ord[next]].status = JOB_READY;
            ENQUEUE(ord[next]);
            next++;
        }
        if (j->remaining > 0) {
            j->status = JOB_READY;
            ENQUEUE(idx);
        } else {
            j->completion = t;
            j->status = JOB_DONE;
            done++;
        }
    }
#undef ENQUEUE
    return 0;
}

/* Effective priority number of a waiting job at time `now` (lower = better). */
static int effective_prio(const Job *j, int now, int aging)
{
    if (aging <= 0)
        return j->priority;
    return j->priority - (now - j->arrival) / aging;
}

static int sim_priority(JobSet *w, int aging, Timeline *tl)
{
    int t = 0, done = 0;
    while (done < w->count) {
        int best = -1;
        for (int i = 0; i < w->count; i++) {
            Job *c = &w->jobs[i];
            if (c->status == JOB_DONE || c->arrival > t)
                continue;
            if (best < 0) {
                best = i;
                continue;
            }
            Job *b = &w->jobs[best];
            int pc = effective_prio(c, t, aging), pb = effective_prio(b, t, aging);
            if (pc < pb || (pc == pb && (c->arrival < b->arrival ||
                (c->arrival == b->arrival && c->id < b->id))))
                best = i;
        }
        if (best < 0) {                                  /* nobody has arrived yet */
            int na = -1;
            for (int i = 0; i < w->count; i++)
                if (w->jobs[i].status != JOB_DONE && (na < 0 || w->jobs[i].arrival < na))
                    na = w->jobs[i].arrival;
            if (add_seg(tl, -1, t, na))
                return -1;
            t = na;
            continue;
        }
        Job *j = &w->jobs[best];
        j->status = JOB_RUNNING;
        j->first_start = t;
        if (add_seg(tl, best, t, t + j->burst))          /* non-preemptive */
            return -1;
        t += j->burst;
        j->remaining = 0;
        j->completion = t;
        j->status = JOB_DONE;
        done++;
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Metrics                                                           */
/* ---------------------------------------------------------------- */

static void finalize(JobSet *w, const Timeline *tl, Metrics *m, const char *label)
{
    memset(m, 0, sizeof *m);
    snprintf(m->label, sizeof m->label, "%s", label);

    double sct = 0, stat = 0, swt = 0, srt = 0;
    for (int i = 0; i < w->count; i++) {
        Job *j = &w->jobs[i];
        j->turnaround = j->completion - j->arrival;
        j->waiting    = j->turnaround - j->burst;
        j->response   = j->first_start - j->arrival;
        sct += j->completion;
        stat += j->turnaround;
        swt += j->waiting;
        srt += j->response;
    }
    m->avg_ct = sct / w->count;
    m->avg_tat = stat / w->count;
    m->avg_wt = swt / w->count;
    m->avg_rt = srt / w->count;

    int busy = 0, prev = -2;
    for (int i = 0; i < tl->n; i++) {
        const Seg *s = &tl->seg[i];
        if (s->job < 0) {
            prev = -2;
            continue;
        }
        busy += s->end - s->start;
        if (prev >= 0 && prev != s->job)
            m->ctx++;
        prev = s->job;
    }
    m->makespan = tl->n ? tl->seg[tl->n - 1].end : 0;
    m->util = m->makespan ? 100.0 * busy / m->makespan : 0.0;
    m->throughput = m->makespan ? (double)w->count / m->makespan : 0.0;
}

/* ---------------------------------------------------------------- */
/* Reporting                                                         */
/* ---------------------------------------------------------------- */

static void print_gantt(const JobSet *w, const Timeline *tl)
{
    printf("Gantt chart (time units):\n");
    for (int base = 0; base < tl->n; base += GANTT_PER_ROW) {
        int end = base + GANTT_PER_ROW < tl->n ? base + GANTT_PER_ROW : tl->n;
        char row1[512] = "", row2[512] = "";
        size_t l1 = 0, l2 = 0;
        for (int i = base; i < end; i++) {
            const Seg *s = &tl->seg[i];
            char cell[40], tm[16];
            snprintf(cell, sizeof cell, " %s", s->job < 0 ? "idle" : w->jobs[s->job].name);
            snprintf(tm, sizeof tm, "%d", s->start);
            int width = (int)strlen(cell) + 1;
            if ((int)strlen(tm) + 1 > width)
                width = (int)strlen(tm) + 1;
            l1 += (size_t)snprintf(row1 + l1, sizeof row1 - l1, "|%-*s", width, cell);
            l2 += (size_t)snprintf(row2 + l2, sizeof row2 - l2, "%-*s", width + 1, tm);
        }
        snprintf(row1 + l1, sizeof row1 - l1, "|");
        snprintf(row2 + l2, sizeof row2 - l2, "%d", tl->seg[end - 1].end);
        printf("  %s\n  %s\n", row1, row2);
    }
    printf("Execution order: ");
    int first = 1;
    for (int i = 0; i < tl->n; i++) {
        if (tl->seg[i].job < 0)
            continue;
        printf("%s%s", first ? "" : " -> ", w->jobs[tl->seg[i].job].name);
        first = 0;
    }
    printf("\n");
}

static void print_report(const char *title, const char *rules, const JobSet *w,
                         const Timeline *tl, const Metrics *m)
{
    print_banner(title);
    printf("%s\n\n", rules);
    print_gantt(w, tl);
    printf("\n  %-4s %-14s %4s %5s %4s | %5s %4s %4s %4s %4s | %s\n", "ID", "Name",
           "Arr", "Burst", "Prio", "Start", "CT", "TAT", "WT", "RT", "Status");
    for (int i = 0; i < w->count; i++) {
        const Job *j = &w->jobs[i];
        printf("  %-4d %-14s %4d %5d %4d | %5d %4d %4d %4d %4d | %s\n", j->id, j->name,
               j->arrival, j->burst, j->priority, j->first_start, j->completion,
               j->turnaround, j->waiting, j->response, job_status_str(j->status));
    }
    printf("\n  Average completion time  : %.2f\n", m->avg_ct);
    printf("  Average turnaround time  : %.2f\n", m->avg_tat);
    printf("  Average waiting time     : %.2f\n", m->avg_wt);
    printf("  Average response time    : %.2f\n", m->avg_rt);
    printf("  Context switches         : %d\n", m->ctx);
    printf("  CPU utilisation          : %.1f%%  (makespan %d)\n", m->util, m->makespan);
    printf("  Throughput               : %.3f jobs/time unit\n", m->throughput);
}

/* ---------------------------------------------------------------- */
/* Policy drivers                                                    */
/* ---------------------------------------------------------------- */

typedef struct {
    JobSet   set;
    Timeline *tl;
    Metrics  m;
} Result;

static const char *FCFS_RULE =
    "Policy: First-Come First-Served (non-preemptive)\n"
    "Tie-break: equal arrival time -> lower job ID runs first.";

static int do_fcfs(const JobSet *base, Result *r)
{
    r->set = *base;
    if (sim_fcfs(&r->set, r->tl))
        return -1;
    finalize(&r->set, r->tl, &r->m, "FCFS");
    return 0;
}

static int do_rr(const JobSet *base, int q, Result *r)
{
    char label[40];
    r->set = *base;
    if (sim_rr(&r->set, q, r->tl))
        return -1;
    snprintf(label, sizeof label, "RR (q=%d)", q);
    finalize(&r->set, r->tl, &r->m, label);
    return 0;
}

static int do_prio(const JobSet *base, int aging, Result *r)
{
    char label[40];
    r->set = *base;
    if (sim_priority(&r->set, aging, r->tl))
        return -1;
    if (aging > 0)
        snprintf(label, sizeof label, "PRIORITY+aging(%d)", aging);
    else
        snprintf(label, sizeof label, "PRIORITY");
    finalize(&r->set, r->tl, &r->m, label);
    return 0;
}

static void rr_rule(char *buf, size_t n, int q)
{
    snprintf(buf, n,
             "Policy: Round Robin (pre-emptive)   TIME QUANTUM = %d\n"
             "Tie-break: a job arriving exactly when a slice ends joins the ready queue\n"
             "           BEFORE the pre-empted job; equal arrivals -> lower job ID first.", q);
}

static void prio_rule(char *buf, size_t n, int aging)
{
    snprintf(buf, n,
             "Policy: Priority (non-preemptive, lower number = higher priority)\n"
             "Tie-break: equal priority -> earlier arrival, then lower job ID.\n"
             "Aging: %s",
             aging > 0 ? "ON - effective priority improves by 1 per N time units waited"
                       : "off");
    if (aging > 0) {
        size_t l = strlen(buf);
        snprintf(buf + l, n - l, " (N=%d)", aging);
    }
}

/* ---------------------------------------------------------------- */
/* Small API used by other modules (ipc.c)                           */
/* ---------------------------------------------------------------- */

const char *policy_short(Policy p)
{
    switch (p) {
    case POLICY_FCFS:     return "FCFS";
    case POLICY_RR:       return "RR";
    case POLICY_PRIORITY: return "PRIORITY";
    }
    return "?";
}

int policy_parse(const char *s, Policy *out)
{
    if (!strcmp(s, "fcfs"))     *out = POLICY_FCFS;
    else if (!strcmp(s, "rr"))  *out = POLICY_RR;
    else if (!strcmp(s, "priority")) *out = POLICY_PRIORITY;
    else return -1;
    return 0;
}

int sched_run(Policy p, const JobSet *set, int quantum, SchedResult *out)
{
    Result r;
    r.tl = calloc(1, sizeof(Timeline));
    if (!r.tl)
        return -1;
    int rc = p == POLICY_FCFS ? do_fcfs(set, &r)
           : p == POLICY_RR   ? do_rr(set, quantum, &r)
                              : do_prio(set, 0, &r);
    if (rc == 0) {
        memcpy(out->jobs, r.set.jobs, sizeof out->jobs);
        out->n = r.set.count;
        out->avg_wt = r.m.avg_wt;
        out->avg_tat = r.m.avg_tat;
    }
    free(r.tl);
    return rc;
}

/* ---------------------------------------------------------------- */
/* compare                                                           */
/* ---------------------------------------------------------------- */

static int run_compare(const JobSet *base, int q, int aging)
{
    Result *res = calloc(4, sizeof *res);
    if (!res) {
        fprintf(stderr, "error: out of memory\n");
        return EXIT_FAIL;
    }
    int n = 0, rc = EXIT_OK;
    for (int i = 0; i < 4; i++)
        res[i].tl = calloc(1, sizeof(Timeline));
    for (int i = 0; i < 4; i++)
        if (!res[i].tl) {
            fprintf(stderr, "error: out of memory\n");
            rc = EXIT_FAIL;
            goto out;
        }

    if (do_fcfs(base, &res[n]) == 0) n++;
    if (do_rr(base, q, &res[n]) == 0) n++;
    if (do_prio(base, 0, &res[n]) == 0) n++;
    if (aging > 0 && do_prio(base, aging, &res[n]) == 0) n++;
    if (n < (aging > 0 ? 4 : 3)) {
        fprintf(stderr, "error: schedule too long for the timeline buffer\n");
        rc = EXIT_FAIL;
        goto out;
    }

    print_banner("SCHEDULING COMPARISON (same job set)");
    printf("Tie-breaks: FCFS/RR/PRIORITY use lower job ID on equal arrival; PRIORITY then\n"
           "prefers earlier arrival on equal priority. RR time quantum = %d.\n\n", q);
    for (int i = 0; i < n; i++) {
        printf("%s\n", res[i].m.label);
        print_gantt(&res[i].set, res[i].tl);
        printf("\n");
    }

    printf("%-20s %7s %7s %7s %7s %5s %7s %10s\n", "Policy", "AvgCT", "AvgTAT",
           "AvgWT", "AvgRT", "Ctx", "CPU%", "Jobs/unit");
    for (int i = 0; i < n; i++) {
        const Metrics *m = &res[i].m;
        printf("%-20s %7.2f %7.2f %7.2f %7.2f %5d %6.1f%% %10.3f\n", m->label, m->avg_ct,
               m->avg_tat, m->avg_wt, m->avg_rt, m->ctx, m->util, m->throughput);
    }

    printf("\nPer-job waiting time:\n  %-4s %-14s", "ID", "Name");
    for (int i = 0; i < n; i++)
        printf(" %18s", res[i].m.label);
    printf("\n");
    for (int k = 0; k < base->count; k++) {
        printf("  %-4d %-14s", base->jobs[k].id, base->jobs[k].name);
        for (int i = 0; i < n; i++)
            printf(" %18d", res[i].set.jobs[k].waiting);
        printf("\n");
    }

    int bwt = 0, btat = 0, brt = 0;
    for (int i = 1; i < n; i++) {
        if (res[i].m.avg_wt  < res[bwt].m.avg_wt)   bwt = i;
        if (res[i].m.avg_tat < res[btat].m.avg_tat) btat = i;
        if (res[i].m.avg_rt  < res[brt].m.avg_rt)   brt = i;
    }
    printf("\nBest average waiting time    : %s (%.2f)\n", res[bwt].m.label, res[bwt].m.avg_wt);
    printf("Best average turnaround time : %s (%.2f)\n", res[btat].m.label, res[btat].m.avg_tat);
    printf("Best average response time   : %s (%.2f)\n", res[brt].m.label, res[brt].m.avg_rt);
    app_log("SCHED", "compare finished: best avg WT = %s (%.2f)", res[bwt].m.label, res[bwt].m.avg_wt);

out:
    for (int i = 0; i < 4; i++)
        free(res[i].tl);
    free(res);
    return rc;
}

/* ---------------------------------------------------------------- */
/* Entry point                                                       */
/* ---------------------------------------------------------------- */

static int sched_usage(void)
{
    fprintf(stderr,
            "usage: campuscompute sched <fcfs|rr|priority|compare> <jobfile> [options]\n"
            "  --quantum N   Round Robin time quantum, 1..50 (default %d)\n"
            "  --aging N     priority aging step, 1..100 (default: off)\n",
            DEFAULT_QUANTUM);
    return EXIT_USAGE;
}

int run_scheduler(int argc, char **argv)
{
    if (argc < 2)
        return sched_usage();
    const char *policy = argv[0], *path = argv[1];
    int quantum = DEFAULT_QUANTUM, aging = 0;

    if (strcmp(policy, "fcfs") && strcmp(policy, "rr") && strcmp(policy, "priority") &&
        strcmp(policy, "compare")) {
        fprintf(stderr, "error: unknown scheduling policy '%s'\n", policy);
        return sched_usage();
    }
    for (int i = 2; i < argc; i++) {
        int r = cc_opt_int(argc, argv, &i, "--quantum", 1, 50, &quantum);
        if (r == 0)
            r = cc_opt_int(argc, argv, &i, "--aging", 1, 100, &aging);
        if (r < 0)
            return sched_usage();
        if (r == 0) {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return sched_usage();
        }
    }

    JobSet base;
    char err[256];
    if (job_load(path, &base, err, sizeof err)) {
        fprintf(stderr, "error: %s\n", err);
        return EXIT_USAGE;
    }
    print_banner("JOB INPUT");
    job_print_input(&base, path);
    app_log("SCHED", "policy=%s file=%s jobs=%d", policy, path, base.count);

    if (!strcmp(policy, "compare"))
        return run_compare(&base, quantum, aging);

    Result r;
    r.tl = calloc(1, sizeof(Timeline));
    if (!r.tl) {
        fprintf(stderr, "error: out of memory\n");
        return EXIT_FAIL;
    }
    char rules[400], title[64];
    int rc = 0;
    if (!strcmp(policy, "fcfs")) {
        rc = do_fcfs(&base, &r);
        snprintf(title, sizeof title, "FCFS SCHEDULING");
        snprintf(rules, sizeof rules, "%s", FCFS_RULE);
    } else if (!strcmp(policy, "rr")) {
        rc = do_rr(&base, quantum, &r);
        snprintf(title, sizeof title, "ROUND ROBIN SCHEDULING");
        rr_rule(rules, sizeof rules, quantum);
    } else {
        rc = do_prio(&base, aging, &r);
        snprintf(title, sizeof title, "PRIORITY SCHEDULING");
        prio_rule(rules, sizeof rules, aging);
    }
    if (rc) {
        fprintf(stderr, "error: schedule too long for the timeline buffer\n");
        free(r.tl);
        return EXIT_FAIL;
    }
    print_report(title, rules, &r.set, r.tl, &r.m);
    app_log("SCHED", "%s done: avgWT=%.2f avgTAT=%.2f", r.m.label, r.m.avg_wt, r.m.avg_tat);
    free(r.tl);
    return EXIT_OK;
}
