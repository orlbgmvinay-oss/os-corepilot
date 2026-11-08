#ifndef BANKER_H
#define BANKER_H

#define BANKER_MAX_PROCESSES 10
#define BANKER_MAX_RESOURCES 10

/* Returns 1 if safe, 0 if unsafe or invalid. sequence receives process indices. */
int bankers_safety(int process_count, int resource_count,
                   const int available[BANKER_MAX_RESOURCES],
                   const int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
                   const int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
                   int sequence[BANKER_MAX_PROCESSES]);
void run_banker_interactive(void);
void banker_demo(void);

#endif
