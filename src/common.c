#include "common.h"

#include <stdio.h>

void corepilot_print_banner(const char *title)
{
    if (title == NULL)
    {
        fputs("Cannot print a banner without a title.\n", stderr);
        return;
    }

    printf("\n=== %s ===\n", title);
}
