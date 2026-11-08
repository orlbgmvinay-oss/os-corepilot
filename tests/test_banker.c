#include "banker.h"

#include <stdio.h>
#include <stdlib.h>

static void check(int condition, const char *expression, const char *file, int line)
{
    if (!condition)
    {
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

int main(void)
{
    const int available[BANKER_MAX_RESOURCES] = {3, 3, 2};
    const int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}
    };
    const int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}
    };
    const int unsafe_available[BANKER_MAX_RESOURCES] = {0};
    const int unsafe_allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {1}, {0}
    };
    const int unsafe_maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {2}, {1}
    };
    int sequence[BANKER_MAX_PROCESSES];

    CHECK(bankers_safety(5, 3, available, allocation, maximum, sequence) == 1);
    CHECK(sequence[0] == 1);
    CHECK(sequence[1] == 3);
    CHECK(sequence[2] == 4);
    CHECK(sequence[3] == 0);
    CHECK(sequence[4] == 2);

    CHECK(bankers_safety(2, 1, unsafe_available, unsafe_allocation,
                         unsafe_maximum, sequence) == 0);
    CHECK(bankers_safety(0, 1, available, allocation, maximum, sequence) == 0);

    puts("Banker tests passed.");
    return 0;
}
