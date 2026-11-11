#include "resource_limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>

int resource_limit_demo(void)
{
    struct rlimit old_limit;
    struct rlimit new_limit;

    int opened_fds[32];
    int count = 0;
    int result = 0;

    printf("\n====================================\n");
    printf("     RESOURCE LIMIT DEMONSTRATION\n");
    printf("====================================\n");

    // Get original resource limit
    if (getrlimit(RLIMIT_NOFILE, &old_limit) == -1)
    {
        perror("getrlimit failed");
        return -1;
    }

    printf("Original soft limit: %ld\n",
           (long)old_limit.rlim_cur);

    printf("Original hard limit: %ld\n",
           (long)old_limit.rlim_max);

    // Set new soft limit without changing hard limit
    new_limit = old_limit;

    if (new_limit.rlim_cur > 32)
        new_limit.rlim_cur = 32;

    if (setrlimit(RLIMIT_NOFILE, &new_limit) == -1)
    {
        perror("setrlimit failed");
        return -1;
    }

    printf("\nNew file descriptor limit: %ld\n",
           (long)new_limit.rlim_cur);

    // Open files until the limit is reached
    printf("\nAttempting to open files...\n");

    while (count < 32)
    {
        int fd = open("/dev/null", O_RDONLY);

        if (fd == -1)
        {
            if (errno == EMFILE)
            {
                printf("\nResource limit reached!\n");
                printf("Error: Too many open files.\n");
            }
            else
            {
                perror("open failed");
                result = -1;
            }

            break;
        }

        opened_fds[count++] = fd;

        printf("File descriptor opened: %d\n", fd);
    }

    printf("\nTotal additional files opened: %d\n", count);

    // Close only descriptors opened by this function
    for (int i = 0; i < count; i++)
    {
        close(opened_fds[i]);
    }

    // Restore original resource limit
    if (setrlimit(RLIMIT_NOFILE, &old_limit) == -1)
    {
        perror("Failed to restore resource limit");
        return -1;
    }

    printf("\nOriginal resource limit restored.\n");
    printf("====================================\n");

    return result;
}
