#include "logger.h"

#include <string.h>
#include <stdlib.h>


/* Consumer thread function */
static void *consumer(void *arg)
{
    Logger *logger = (Logger *)arg;

    while (true)
    {
        pthread_mutex_lock(&logger->mutex);

        /* Wait while the queue is empty */
        while (logger->count == 0 && !logger->stopping)
        {
            logger->consumer_waits++;

            pthread_cond_wait(
                &logger->not_empty,
                &logger->mutex
            );
        }

        /* Stop only when no pending events remain */
        if (logger->count == 0 && logger->stopping)
        {
            pthread_mutex_unlock(&logger->mutex);
            break;
        }

        /* Remove event from queue */
        LogEvent event = logger->queue[logger->front];

        logger->front =
            (logger->front + 1) % LOGGER_QUEUE_CAPACITY;

        logger->count--;

        /* Inform producers that space is available */
        pthread_cond_signal(&logger->not_full);

        pthread_mutex_unlock(&logger->mutex);

        /* Write event into log file */
        fprintf(logger->log_file, "%s\n", event.message);

        fflush(logger->log_file);
    }

    return NULL;
}


/* Initialize logger */
int logger_init(Logger *logger, const char *filename)
{
    if (logger == NULL || filename == NULL)
        return -1;

    memset(logger, 0, sizeof(Logger));

    logger->log_file = fopen(filename, "a");

    if (logger->log_file == NULL)
        return -1;

    if (pthread_mutex_init(&logger->mutex, NULL) != 0)
    {
        fclose(logger->log_file);
        logger->log_file = NULL;
        return -1;
    }

    if (pthread_cond_init(&logger->not_full, NULL) != 0)
    {
        pthread_mutex_destroy(&logger->mutex);
        fclose(logger->log_file);
        logger->log_file = NULL;
        return -1;
    }

    if (pthread_cond_init(&logger->not_empty, NULL) != 0)
    {
        pthread_cond_destroy(&logger->not_full);
        pthread_mutex_destroy(&logger->mutex);
        fclose(logger->log_file);
        logger->log_file = NULL;
        return -1;
    }

    logger->front = 0;
    logger->rear = 0;
    logger->count = 0;

    logger->stopping = false;
    logger->started = false;

    return 0;
}


/* Start consumer thread */
int logger_start(Logger *logger)
{
    if (logger == NULL || logger->log_file == NULL ||
        logger->started)
        return -1;

    if (pthread_create(
            &logger->consumer_thread,
            NULL,
            consumer,
            logger) != 0)
    {
        return -1;
    }

    logger->started = true;

    return 0;
}


/* Producer: Add event to queue */
int logger_log(Logger *logger, const char *message)
{
    if (logger == NULL || message == NULL)
        return -1;

    if (strlen(message) >= LOGGER_MESSAGE_SIZE)
        return -1;

    pthread_mutex_lock(&logger->mutex);

    /* Wait while queue is full */
    while (logger->count == LOGGER_QUEUE_CAPACITY &&
            !logger->stopping)
    {
        logger->producer_waits++;

        pthread_cond_wait(
            &logger->not_full,
            &logger->mutex
        );
    }

    /* Reject new events during shutdown */
    if (logger->stopping)
    {
        pthread_mutex_unlock(&logger->mutex);
        return -1;
    }

    /* Insert event into circular queue */
    snprintf(
        logger->queue[logger->rear].message,
        LOGGER_MESSAGE_SIZE,
        "%s",
        message
    );

    logger->rear =
        (logger->rear + 1) % LOGGER_QUEUE_CAPACITY;

    logger->count++;

    /* Inform consumer that data is available */
    pthread_cond_signal(&logger->not_empty);

    pthread_mutex_unlock(&logger->mutex);

    return 0;
}


/* Stop consumer thread */
void logger_stop(Logger *logger)
{
    if (logger == NULL || !logger->started)
        return;

    pthread_mutex_lock(&logger->mutex);

    logger->stopping = true;

    /* Wake up waiting threads */
    pthread_cond_broadcast(&logger->not_empty);
    pthread_cond_broadcast(&logger->not_full);

    pthread_mutex_unlock(&logger->mutex);

    pthread_join(logger->consumer_thread, NULL);

    logger->started = false;
}


/* Destroy logger resources */
void logger_destroy(Logger *logger)
{
    if (logger == NULL || logger->log_file == NULL)
        return;

    logger_stop(logger);

    pthread_cond_destroy(&logger->not_empty);
    pthread_cond_destroy(&logger->not_full);

    pthread_mutex_destroy(&logger->mutex);

    fclose(logger->log_file);

    logger->log_file = NULL;
}