#include "logger.h"
#include <stdio.h>
#include <pthread.h>

#define PRODUCER_COUNT 3

typedef struct
{
    Logger *logger;
    int producer_id;
} ProducerData;


/* Producer thread function */
void *producer(void *arg)
{
    ProducerData *data = (ProducerData *)arg;

    char message[LOGGER_MESSAGE_SIZE];

    for (int i = 1; i <= 5000; i++)
    {
        snprintf(message, sizeof(message),
                 "Producer %d generated event %d",
                 data->producer_id, i);

        logger_log(data->logger, message);
    }

    return NULL;
}


int main()
{
    Logger logger;

    pthread_t producers[PRODUCER_COUNT];
    ProducerData data[PRODUCER_COUNT];

    if (logger_init(&logger, "logs/campus.log") != 0)
    {
        printf("Logger initialization failed!\n");
        return 1;
    }

    if (logger_start(&logger) != 0)
    {
        printf("Consumer thread creation failed!\n");
        logger_destroy(&logger);
        return 1;
    }

    /* Create producer threads */
    for (int i = 0; i < PRODUCER_COUNT; i++)
    {
        data[i].logger = &logger;
        data[i].producer_id = i + 1;

        if (pthread_create(&producers[i], NULL,
                           producer, &data[i]) != 0)
        {
            printf("Producer thread creation failed!\n");

            /* Wait for previously created producers */
            for (int j = 0; j < i; j++)
                pthread_join(producers[j], NULL);

            logger_stop(&logger);

            printf("\n--- Synchronization Statistics ---\n");
            printf("Producer wait count: %d\n", logger.producer_waits);
            printf("Consumer wait count: %d\n", logger.consumer_waits);

            logger_destroy(&logger);
            return 1;
        }
    }

    /* Wait for all producers */
    for (int i = 0; i < PRODUCER_COUNT; i++)
    {
        pthread_join(producers[i], NULL);
    }

logger_stop(&logger);

printf("\n--- Synchronization Statistics ---\n");
printf("Producer wait count: %d\n", logger.producer_waits);
printf("Consumer wait count: %d\n", logger.consumer_waits);

logger_destroy(&logger);

printf("Multiple producer test completed successfully!\n");

    printf("Multiple producer test completed successfully!\n");

    return 0;
}