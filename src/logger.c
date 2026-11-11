/*
 * logger.c - FR3: concurrent activity logger.
 *
 * Design
 *   * A fixed-capacity circular buffer (BoundedQueue) is shared by producer
 *     threads (lab events) and consumer threads (log writers).
 *   * One mutex protects all queue fields. Two condition variables coordinate
 *     the full/empty conditions:
 *         not_full  - producers wait here while count == capacity
 *         not_empty - consumers wait here while count == 0
 *     Every wait sits in a `while (predicate)` loop, so spurious wakeups and
 *     races between several consumers are harmless.
 *   * Shutdown: each producer decrements `producers_active` when it is done;
 *     the last one broadcasts not_empty. A consumer exits only when the queue
 *     is empty AND no producer is still active, so nothing is lost.
 *   * Every item carries a unique ID. After the threads are joined, a
 *     "seen" table proves each item was consumed exactly once.
 */
#include "logger.h"
#include "common.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ===================== bounded producer-consumer queue =================== */

#define MAX_THREADS 8

typedef struct {
    int  id;                 /* globally unique item number            */
    int  producer;           /* 1-based producer label                 */
    char text[112];
} LogItem;

typedef struct {
    LogItem        *buf;
    int             capacity, head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
    int             producers_active;
    int             verbose;
    long            produced, consumed, full_waits, empty_waits;
    int             max_depth;
    unsigned char  *seen;
    int             total;
} BoundedQueue;

typedef struct {
    BoundedQueue *q;
    int           index;      /* 0-based thread index                  */
    int           items;      /* items to produce (producers only)     */
} ThreadArg;

static const char *const EVENT_TEXT[] = {
    "job submitted to the scheduler queue",
    "lab node reported CPU usage above 90%",
    "student session opened on a lab node",
    "simulation checkpoint written to disk",
    "job finished and results archived",
    "resource request evaluated by the safety check",
};
#define EVENT_TEXT_COUNT (sizeof EVENT_TEXT / sizeof EVENT_TEXT[0])

#define TRACE(q, ...)                      \
    do {                                   \
        if ((q)->verbose)                  \
            printf(__VA_ARGS__);           \
    } while (0)

static int queue_init(BoundedQueue *q, int capacity, int producers, int total,
                      int verbose)
{
    memset(q, 0, sizeof *q);
    q->buf = calloc((size_t)capacity, sizeof *q->buf);
    q->seen = calloc((size_t)total, 1);
    if (q->buf == NULL || q->seen == NULL) {
        free(q->buf);
        free(q->seen);
        return -1;
    }
    q->capacity = capacity;
    q->producers_active = producers;
    q->total = total;
    q->verbose = verbose;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return 0;
}

static void queue_destroy(BoundedQueue *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    free(q->buf);
    free(q->seen);
}

static void *producer_main(void *arg)
{
    ThreadArg *a = arg;
    BoundedQueue *q = a->q;
    int label = a->index + 1;

    for (int i = 0; i < a->items; i++) {
        LogItem item;

        item.id = a->index * a->items + i;
        item.producer = label;
        snprintf(item.text, sizeof item.text, "%s (user-%d, event %d)",
                 EVENT_TEXT[(size_t)(item.id) % EVENT_TEXT_COUNT], label, i + 1);

        pthread_mutex_lock(&q->lock);
        if (q->count == q->capacity) {
            q->full_waits++;
            TRACE(q, "[BLOCKED ] P%d waits: queue FULL (%d/%d)\n", label,
                  q->count, q->capacity);
        }
        while (q->count == q->capacity)
            pthread_cond_wait(&q->not_full, &q->lock);

        q->buf[q->tail] = item;
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;
        q->produced++;
        if (q->count > q->max_depth)
            q->max_depth = q->count;
        TRACE(q, "[PRODUCE ] P%d -> item #%02d  (queue %d/%d)\n", label,
              item.id, q->count, q->capacity);
        pthread_cond_signal(&q->not_empty);
        pthread_mutex_unlock(&q->lock);

        sleep_ms(3 + 2 * a->index);        /* simulate work between events */
    }

    pthread_mutex_lock(&q->lock);
    q->producers_active--;
    TRACE(q, "[FINISHED] P%d produced all %d items\n", label, a->items);
    if (q->producers_active == 0)
        pthread_cond_broadcast(&q->not_empty);   /* wake consumers to exit */
    pthread_mutex_unlock(&q->lock);
    return NULL;
}

static void *consumer_main(void *arg)
{
    ThreadArg *a = arg;
    BoundedQueue *q = a->q;
    int label = a->index + 1;
    long handled = 0;

    for (;;) {
        LogItem item;

        pthread_mutex_lock(&q->lock);
        if (q->count == 0 && q->producers_active > 0) {
            q->empty_waits++;
            TRACE(q, "[BLOCKED ] C%d waits: queue EMPTY (0/%d)\n", label,
                  q->capacity);
        }
        while (q->count == 0 && q->producers_active > 0)
            pthread_cond_wait(&q->not_empty, &q->lock);

        if (q->count == 0) {                 /* drained and producers done */
            pthread_mutex_unlock(&q->lock);
            break;
        }
        item = q->buf[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        q->consumed++;
        q->seen[item.id]++;
        TRACE(q, "[CONSUME ] C%d <- item #%02d from P%d  (queue %d/%d)\n", label,
              item.id, item.producer, q->count, q->capacity);
        pthread_cond_signal(&q->not_full);
        pthread_mutex_unlock(&q->lock);

        /* Writing to the log file happens outside the queue lock. */
        log_event("EVENT", "#%02d from P%d via C%d: %s", item.id, item.producer,
                  label, item.text);
        handled++;
        sleep_ms(12);                        /* consumers are slower on purpose */
    }
    TRACE(q, "[FINISHED] C%d exiting after writing %ld entries\n", label, handled);
    return NULL;
}

int logger_demo(const LoggerConfig *cfg, int verbose)
{
    BoundedQueue q;
    pthread_t prod[MAX_THREADS], cons[MAX_THREADS];
    ThreadArg pa[MAX_THREADS], ca[MAX_THREADS];
    int np = 0, nc = 0, ok = 1;
    int total = cfg->producers * cfg->items_per_producer;
    int lost = 0, duplicated = 0;

    if (queue_init(&q, cfg->capacity, cfg->producers, total, verbose) != 0) {
        print_error("out of memory while creating the log queue");
        return 0;
    }
    log_event("LOGGER", "start: %d producer(s), %d consumer(s), %d item(s) each, capacity %d",
              cfg->producers, cfg->consumers, cfg->items_per_producer, cfg->capacity);

    /* Consumers start first so the "queue EMPTY" wait is visible. */
    for (int i = 0; i < cfg->consumers && ok; i++) {
        int rc;

        ca[i] = (ThreadArg){ &q, i, 0 };
        rc = pthread_create(&cons[nc], NULL, consumer_main, &ca[i]);
        if (rc != 0) {
            print_error("pthread_create (consumer %d): %s", i + 1, strerror(rc));
            ok = 0;
        } else {
            nc++;
        }
    }
    for (int i = 0; i < cfg->producers && ok; i++) {
        int rc;

        pa[i] = (ThreadArg){ &q, i, cfg->items_per_producer };
        rc = pthread_create(&prod[np], NULL, producer_main, &pa[i]);
        if (rc != 0) {
            print_error("pthread_create (producer %d): %s", i + 1, strerror(rc));
            ok = 0;
        } else {
            np++;
        }
    }
    if (!ok) {
        /* Producers that never started must not keep consumers waiting. */
        pthread_mutex_lock(&q.lock);
        q.producers_active -= cfg->producers - np;
        pthread_cond_broadcast(&q.not_empty);
        pthread_mutex_unlock(&q.lock);
    }

    for (int i = 0; i < np; i++) {
        pthread_join(prod[i], NULL);
        printf("[JOINED  ] producer thread P%d joined\n", i + 1);
    }
    for (int i = 0; i < nc; i++) {
        pthread_join(cons[i], NULL);
        printf("[JOINED  ] consumer thread C%d joined\n", i + 1);
    }

    for (int i = 0; i < total; i++) {
        if (q.seen[i] == 0)
            lost++;
        else if (q.seen[i] > 1)
            duplicated++;
    }
    print_section("Logger statistics");
    printf("  Items produced        : %ld\n", q.produced);
    printf("  Items consumed        : %ld\n", q.consumed);
    printf("  Peak queue depth      : %d / %d\n", q.max_depth, q.capacity);
    printf("  Producer waits (FULL) : %ld\n", q.full_waits);
    printf("  Consumer waits (EMPTY): %ld\n", q.empty_waits);
    printf("  Threads joined        : %d producer(s), %d consumer(s)\n", np, nc);
    printf("  Threads still alive   : %d (main thread only)\n", cc_thread_count());

    ok = ok && lost == 0 && duplicated == 0 && q.produced == total &&
         q.consumed == total;
    printf("  Integrity check       : %s (%d expected, %ld consumed, %d lost, %d duplicated)\n",
           ok ? "PASS" : "FAIL", total, q.consumed, lost, duplicated);
    log_event("LOGGER", "finished: produced=%ld consumed=%ld lost=%d duplicated=%d peak=%d result=%s",
              q.produced, q.consumed, lost, duplicated, q.max_depth,
              ok ? "PASS" : "FAIL");

    queue_destroy(&q);
    return ok;
}

static void usage_logger(FILE *out)
{
    fprintf(out,
            "usage: campuscompute logger [--producers N] [--consumers N] [--items N] [--capacity N]\n"
            "  Runs the bounded producer-consumer logger (pthread mutex + condition variables).\n"
            "  --producers N  producer threads, 1-%d   (default 3)\n"
            "  --consumers N  consumer threads, 1-%d   (default 2)\n"
            "  --items N      items per producer, 1-500 (default 5)\n"
            "  --capacity N   queue slots, 1-64         (default 4)\n"
            "  Output is written to %s\n", MAX_THREADS, MAX_THREADS, LOG_PATH);
}

int run_logger_cmd(int argc, char **argv)
{
    LoggerConfig cfg = { 3, 2, 5, 4 };
    int ok;

    for (int i = 0; i < argc; i++) {
        const char *opt = argv[i];
        int *target = NULL, min = 1, max = 0;

        if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
            usage_logger(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(opt, "--producers") == 0) {
            target = &cfg.producers; max = MAX_THREADS;
        } else if (strcmp(opt, "--consumers") == 0) {
            target = &cfg.consumers; max = MAX_THREADS;
        } else if (strcmp(opt, "--items") == 0) {
            target = &cfg.items_per_producer; max = 500;
        } else if (strcmp(opt, "--capacity") == 0) {
            target = &cfg.capacity; max = 64;
        } else {
            print_error("unknown option '%s'", opt);
            usage_logger(stderr);
            return EXIT_USAGE;
        }
        if (i + 1 >= argc) {
            print_error("option '%s' needs a value", opt);
            usage_logger(stderr);
            return EXIT_USAGE;
        }
        if (parse_int_arg(argv[++i], min, max, target) != 0) {
            print_error("invalid value '%s' for %s (expected %d-%d)", argv[i], opt, min, max);
            usage_logger(stderr);
            return EXIT_USAGE;
        }
    }

    print_banner("Concurrent activity logger (producer-consumer)");
    printf("  %d producer(s), %d consumer(s), %d item(s) each, queue capacity %d\n\n",
           cfg.producers, cfg.consumers, cfg.items_per_producer, cfg.capacity);

    long before = cc_count_lines(LOG_PATH);
    ok = logger_demo(&cfg, 1);

    print_section("Generated log file");
    printf("  File: %s (%ld line(s) total, %ld added by this run); first entries of this run:\n",
           LOG_PATH, cc_count_lines(LOG_PATH), cc_count_lines(LOG_PATH) - before);
    cc_print_lines(LOG_PATH, before, 8);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
