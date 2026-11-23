/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/student.h"

#include <stdio.h>

const char *student_paper_name(size_t index)
{
    static const char *names[SRMS_PAPER_COUNT] = {
        "Paper I",
        "Paper II",
        "Paper III",
        "Paper IV",
        "Practical"
    };

    if (index >= SRMS_PAPER_COUNT) {
        return "";
    }
    return names[index];
}

uint16_t student_percentage_tenths(const Student *student)
{
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i < SRMS_PAPER_COUNT; i++) {
        sum += student->marks_tenths[i];
    }
    return (uint16_t)((sum + 2u) / 5u);
}

char student_grade(const Student *student)
{
    uint16_t pct = student_percentage_tenths(student);

    if (pct >= 900) {
        return 'A';
    }
    if (pct >= 800) {
        return 'B';
    }
    if (pct >= 700) {
        return 'C';
    }
    if (pct >= 600) {
        return 'D';
    }
    if (pct >= 500) {
        return 'E';
    }
    return 'F';
}

int student_grade_index(char grade)
{
    switch (grade) {
    case 'A':
        return 0;
    case 'B':
        return 1;
    case 'C':
        return 2;
    case 'D':
        return 3;
    case 'E':
        return 4;
    case 'F':
        return 5;
    default:
        return -1;
    }
}

void student_format_roll(char *dst, size_t cap, uint32_t roll)
{
    if (roll < 10000u) {
        snprintf(dst, cap, "%04u", (unsigned)roll);
    } else {
        snprintf(dst, cap, "%06u", (unsigned)roll);
    }
}

void student_format_tenths(char *dst, size_t cap, uint16_t tenths)
{
    snprintf(dst, cap, "%u.%u", (unsigned)(tenths / 10u), (unsigned)(tenths % 10u));
}
