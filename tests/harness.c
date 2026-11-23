/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "harness.h"

#include <stdio.h>

static int g_failures;
static int g_passes;

void srms_check(int condition, const char *file, int line, const char *expr)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
        g_failures++;
    } else {
        g_passes++;
    }
}

int srms_check_failures(void)
{
    return g_failures;
}

int srms_check_passes(void)
{
    return g_passes;
}
