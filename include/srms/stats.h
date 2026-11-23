/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_STATS_H
#define SRMS_STATS_H

#include "srms/registry.h"

#include <stdio.h>

#define SRMS_GRADE_COUNT 6
#define SRMS_PCT_BIN_COUNT 6
#define SRMS_TOP_COUNT 3

typedef struct ClassStats {
    size_t count;
    uint32_t average_tenths;
    uint32_t median_tenths;
    const Student *highest;
    const Student *lowest;
    size_t grade_count[SRMS_GRADE_COUNT];
    /* 0 below 50, then decades through 90–100. */
    size_t pct_bins[SRMS_PCT_BIN_COUNT];
    const Student *top[SRMS_TOP_COUNT];
    size_t top_count;
} ClassStats;

void stats_compute(const Registry *registry, ClassStats *stats);
void stats_write_report(FILE *out, const Registry *registry, const ClassStats *stats);

#endif
