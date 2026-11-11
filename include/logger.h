/*
 * logger.h - FR3: bounded producer-consumer logging pipeline (pthreads).
 * The event log itself is common.c's app_log(); log_event is an alias.
 */
#ifndef CAMPUS_LOGGER_H
#define CAMPUS_LOGGER_H

#include "common.h"

#define log_event app_log

typedef struct {
    int producers;
    int consumers;
    int items_per_producer;
    int capacity;
} LoggerConfig;

/* Runs the pipeline; returns 1 when every item was consumed exactly once. */
int logger_demo(const LoggerConfig *cfg, int verbose);
int run_logger_cmd(int argc, char **argv);

#endif /* CAMPUS_LOGGER_H */
