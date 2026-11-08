#include "banker.h"
#include <stdio.h>

static int calculate_need(int n, int m,
    const int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
    const int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
    int need[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES]) {
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < m; ++j) {
            if (allocation[i][j] < 0 || maximum[i][j] < allocation[i][j]) return 0;
            need[i][j] = maximum[i][j] - allocation[i][j];
        }
    return 1;
}

int bankers_safety(int n, int m, const int available[BANKER_MAX_RESOURCES],
    const int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
    const int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES],
    int sequence[BANKER_MAX_PROCESSES]) {
    if (n < 1 || n > BANKER_MAX_PROCESSES || m < 1 || m > BANKER_MAX_RESOURCES ||
        !available || !allocation || !maximum || !sequence) return 0;

    int work[BANKER_MAX_RESOURCES], finish[BANKER_MAX_PROCESSES] = {0};
    int need[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {{0}};
    for (int j = 0; j < m; ++j) {
        if (available[j] < 0) return 0;
        work[j] = available[j];
    }
    if (!calculate_need(n, m, allocation, maximum, need)) {
        printf("Invalid Banker input: Maximum must be >= Allocation and values non-negative.\n");
        return 0;
    }

    printf("\nNeed matrix (Maximum - Allocation):\n");
    for (int i = 0; i < n; ++i) {
        printf("P%d: ", i);
        for (int j = 0; j < m; ++j) printf("%d ", need[i][j]);
        printf("\n");
    }

    int found = 0;
    while (found < n) {
        int progress = 0;
        for (int i = 0; i < n; ++i) {
            if (finish[i]) continue;
            int possible = 1;
            for (int j = 0; j < m; ++j)
                if (need[i][j] > work[j]) { possible = 0; break; }
            if (possible) {
                for (int j = 0; j < m; ++j) work[j] += allocation[i][j];
                sequence[found++] = i;
                finish[i] = 1;
                progress = 1;
            }
        }
        if (!progress) break;
    }

    if (found == n) {
        printf("SAFE state. Safe sequence: ");
        for (int i = 0; i < n; ++i) printf("P%d%s", sequence[i], i == n - 1 ? "\n" : " -> ");
        return 1;
    }
    printf("UNSAFE state. No safe sequence exists for the supplied snapshot.\n");
    return 0;
}

void run_banker_interactive(void) {
    int n, m, available[BANKER_MAX_RESOURCES];
    int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {{0}};
    int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {{0}};
    int sequence[BANKER_MAX_PROCESSES];
    printf("Number of processes (1-%d): ", BANKER_MAX_PROCESSES);
    if (scanf("%d", &n) != 1 || n < 1 || n > BANKER_MAX_PROCESSES) { printf("Invalid process count.\n"); return; }
    printf("Number of resource types (1-%d): ", BANKER_MAX_RESOURCES);
    if (scanf("%d", &m) != 1 || m < 1 || m > BANKER_MAX_RESOURCES) { printf("Invalid resource count.\n"); return; }
    printf("Enter Available vector (%d integers): ", m);
    for (int j = 0; j < m; ++j)
        if (scanf("%d", &available[j]) != 1 || available[j] < 0) { printf("Invalid Available value.\n"); return; }
    printf("Enter Allocation matrix, row by row (%d x %d):\n", n, m);
    for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j)
        if (scanf("%d", &allocation[i][j]) != 1 || allocation[i][j] < 0) { printf("Invalid Allocation value.\n"); return; }
    printf("Enter Maximum matrix, row by row (%d x %d):\n", n, m);
    for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j)
        if (scanf("%d", &maximum[i][j]) != 1 || maximum[i][j] < 0) { printf("Invalid Maximum value.\n"); return; }
    bankers_safety(n, m, available,
                   (const int (*)[BANKER_MAX_RESOURCES])allocation,
                   (const int (*)[BANKER_MAX_RESOURCES])maximum, sequence);
}

void banker_demo(void) {
    const int available[BANKER_MAX_RESOURCES] = {3, 3, 2};
    const int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}
    };
    const int maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}
    };
    int sequence[BANKER_MAX_PROCESSES];
    printf("Banker's Algorithm standard SAFE example:\n");
    bankers_safety(5, 3, available, allocation, maximum, sequence);

    const int unsafe_available[BANKER_MAX_RESOURCES] = {0};
    const int unsafe_allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {1}, {0}
    };
    const int unsafe_maximum[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES] = {
        {2}, {1}
    };
    printf("\nBanker's Algorithm UNSAFE example:\n");
    bankers_safety(2, 1, unsafe_available, unsafe_allocation, unsafe_maximum, sequence);
}
