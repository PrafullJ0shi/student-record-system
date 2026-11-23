/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/stats.h"

#include "internal.h"

#include <stdlib.h>
#include <string.h>

static int cmp_u16(const void *left, const void *right)
{
    uint16_t a = *(const uint16_t *)left;
    uint16_t b = *(const uint16_t *)right;
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int ranks_above(const Student *left, const Student *right)
{
    uint16_t lp = student_percentage_tenths(left);
    uint16_t rp = student_percentage_tenths(right);
    int name;

    if (lp != rp) {
        return lp > rp;
    }
    name = srms_ascii_casecmp(left->name, right->name);
    if (name != 0) {
        return name < 0;
    }
    return left->roll < right->roll;
}

static size_t percentage_bin(uint16_t tenths)
{
    if (tenths < 500u) {
        return 0;
    }
    if (tenths >= 1000u) {
        return 5;
    }
    return (size_t)(tenths / 100u) - 4u;
}

static void consider_top(ClassStats *stats, const Student *student)
{
    size_t i;
    size_t slot = stats->top_count;

    for (i = 0; i < stats->top_count; i++) {
        if (ranks_above(student, stats->top[i])) {
            slot = i;
            break;
        }
    }
    if (slot >= SRMS_TOP_COUNT) {
        return;
    }
    if (stats->top_count < SRMS_TOP_COUNT) {
        stats->top_count++;
    }
    for (i = stats->top_count - 1u; i > slot; i--) {
        stats->top[i] = stats->top[i - 1u];
    }
    stats->top[slot] = student;
}

void stats_compute(const Registry *registry, ClassStats *stats)
{
    const RegistryNode *node;
    uint16_t *values;
    uint64_t sum = 0;
    size_t n = 0;

    memset(stats, 0, sizeof *stats);
    stats->count = registry->count;
    if (registry->count == 0) {
        return;
    }
    values = malloc(registry->count * sizeof *values);
    if (values == NULL) {
        stats->count = 0;
        return;
    }
    for (node = registry->head; node != NULL; node = node->next) {
        uint16_t pct = student_percentage_tenths(&node->student);
        char grade = student_grade(&node->student);
        int index = student_grade_index(grade);
        values[n++] = pct;
        sum += pct;
        if (index >= 0) {
            stats->grade_count[index]++;
        }
        stats->pct_bins[percentage_bin(pct)]++;
        if (stats->highest == NULL || ranks_above(&node->student, stats->highest)) {
            stats->highest = &node->student;
        }
        if (stats->lowest == NULL || ranks_above(stats->lowest, &node->student)) {
            stats->lowest = &node->student;
        }
        consider_top(stats, &node->student);
    }
    stats->average_tenths = (uint32_t)((sum + (uint64_t)n / 2u) / (uint64_t)n);
    qsort(values, n, sizeof *values, cmp_u16);
    if ((n % 2u) == 1u) {
        stats->median_tenths = values[n / 2u];
    } else {
        uint32_t left = values[n / 2u - 1u];
        uint32_t right = values[n / 2u];
        stats->median_tenths = (left + right + 1u) / 2u;
    }
    free(values);
}

static void write_student_line(FILE *out, const char *key, const Student *student)
{
    char roll[16];

    if (student == NULL) {
        return;
    }
    student_format_roll(roll, sizeof roll, student->roll);
    fprintf(out, "%s_roll\t%s\n", key, roll);
    fprintf(out, "%s_name\t%s\n", key, student->name);
    fprintf(out, "%s_programme\t%s\n", key, student->programme);
    fprintf(out, "%s_tenths\t%u\n", key, (unsigned)student_percentage_tenths(student));
    fprintf(out, "%s_grade\t%c\n", key, student_grade(student));
}

void stats_write_report(FILE *out, const Registry *registry, const ClassStats *stats)
{
    static const char grades[] = {'A', 'B', 'C', 'D', 'E', 'F'};
    const RegistryNode *node;
    size_t i;

    fprintf(out, "count\t%zu\n", stats->count);
    fprintf(out, "average_tenths\t%u\n", (unsigned)stats->average_tenths);
    fprintf(out, "median_tenths\t%u\n", (unsigned)stats->median_tenths);
    write_student_line(out, "highest", stats->highest);
    write_student_line(out, "lowest", stats->lowest);
    for (i = 0; i < SRMS_GRADE_COUNT; i++) {
        fprintf(out, "grade\t%c\t%zu\n", grades[i], stats->grade_count[i]);
    }
    for (i = 0; i < SRMS_PCT_BIN_COUNT; i++) {
        fprintf(out, "bin\t%zu\t%zu\n", i, stats->pct_bins[i]);
    }
    for (i = 0; i < stats->top_count; i++) {
        const Student *student = stats->top[i];
        fprintf(out, "top\t%zu\t%u\t%s\t%s\t%u\t%c\n",
                i + 1u,
                (unsigned)student->roll,
                student->name,
                student->programme,
                (unsigned)student_percentage_tenths(student),
                student_grade(student));
    }
    for (node = registry->head; node != NULL; node = node->next) {
        const Student *student = &node->student;
        fprintf(out, "record\t%u\t%s\t%s\t%u\t%u\t%c\n",
                (unsigned)student->roll,
                student->name,
                student->programme,
                (unsigned)student->year,
                (unsigned)student_percentage_tenths(student),
                student_grade(student));
    }
}
