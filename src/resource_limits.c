#include "resource_limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>

// ==========================================
// Demonstrate Process Resource Limits
// ==========================================

int resource_limit_demo(void)
{
    struct rlimit limit;
    struct rlimit old_limit;

    int fd;
    int count = 0;

    printf("\n====================================\n");
    printf("     RESOURCE LIMIT DEMONSTRATION\n");
    printf("====================================\n");

    // Get the current file descriptor limit
    if (getrlimit(RLIMIT_NOFILE, &old_limit) == -1)
    {
        perror("getrlimit failed");
        return -1;
    }

    printf("Original soft limit: %ld\n",
           (long)old_limit.rlim_cur);

    printf("Original hard limit: %ld\n",
           (long)old_limit.rlim_max);

    // Set a new resource limit
    limit.rlim_cur = 32;

    // Never increase the existing hard limit
    limit.rlim_max = old_limit.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &limit) == -1)
    {
        perror("setrlimit failed");
        return -1;
    }

    printf("\nNew file descriptor limit applied: 32\n");

    // Open files until the resource limit is reached
    printf("\nAttempting to open files...\n");

    while (1)
    {
        fd = open("/dev/null", O_RDONLY);

        if (fd == -1)
        {
            if (errno == EMFILE)
            {
                printf("\nResource limit reached!\n");
                printf("Error: Too many open files.\n");
                break;
            }

            perror("open failed");

            // Close already opened descriptors
            for (int i = 0; i < count; i++)
            {
                // Handled separately below
            }

            return -1;
        }

        count++;

        printf("File descriptor opened: %d\n", fd);
    }

    printf("\nTotal additional files opened: %d\n", count);

    // Close the opened file descriptors
    // Standard descriptors 0, 1 and 2 are preserved.
    for (int fd_to_close = 3; fd_to_close < 32; fd_to_close++)
    {
        close(fd_to_close);
    }

    // Restore the original resource limit
    if (setrlimit(RLIMIT_NOFILE, &old_limit) == -1)
    {
        perror("Failed to restore resource limit");
        return -1;
    }

    printf("\nOriginal resource limit restored.\n");

    printf("====================================\n");

    return 0;
}