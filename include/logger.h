#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

/* Maximum number of events in the queue */
#define LOGGER_QUEUE_CAPACITY 10

/* Maximum length of each log message */
#define LOGGER_MESSAGE_SIZE 256

/* Structure representing one log event */
typedef struct
{
    char message[LOGGER_MESSAGE_SIZE];
} LogEvent;

/* Logger structure */
typedef struct
{
    LogEvent queue[LOGGER_QUEUE_CAPACITY];

    int front;
    int rear;
    int count;
    int producer_waits;
    int consumer_waits;

    pthread_mutex_t mutex;

    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    pthread_t consumer_thread;

    FILE *log_file;

    bool stopping;
    bool started;

} Logger;


/* Initialize the logger */
int logger_init(Logger *logger, const char *filename);

/* Start the consumer thread */
int logger_start(Logger *logger);

/* Add an event to the queue */
int logger_log(Logger *logger, const char *message);

/* Stop the logger and wait for the consumer */
void logger_stop(Logger *logger);

/* Release logger resources */
void logger_destroy(Logger *logger);

#endif