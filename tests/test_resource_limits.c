#include "resource_limits.h"
#include <stdio.h>

int main()
{
    printf("Starting Resource Limit Test...\n");

    if (resource_limit_demo() == 0)
    {
        printf("\nFR7 TEST: PASSED\n");
    }
    else
    {
        printf("\nFR7 TEST: FAILED\n");
        return 1;
    }

    return 0;
}