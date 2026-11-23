/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_STUDENT_H
#define SRMS_STUDENT_H

#include <stddef.h>
#include <stdint.h>

#define SRMS_VERSION "1.0.0"
#define SRMS_FORMAT_VERSION 1

#define SRMS_NAME_CAP 40
#define SRMS_PROGRAMME_CAP 24
#define SRMS_PAPER_COUNT 5

#define SRMS_YEAR_MIN 1970
#define SRMS_YEAR_MAX 2100
#define SRMS_MARK_TENTHS_MAX 1000
#define SRMS_ROLL_MAX 999999u
#define SRMS_MAX_RECORDS 10000u
#define SRMS_MAX_FILE_BYTES (2u * 1024u * 1024u)

typedef struct Student {
    uint32_t roll;
    char name[SRMS_NAME_CAP];
    char programme[SRMS_PROGRAMME_CAP];
    uint16_t year;
    uint16_t marks_tenths[SRMS_PAPER_COUNT];
} Student;

const char *student_paper_name(size_t index);

/* Mean of the five papers, in tenths of a percent, rounded half up.
 * The mean of tenths lands on a fifth, so the second decimal is never 5.
 */
uint16_t student_percentage_tenths(const Student *student);

/* A from 90, B from 80, C from 70, D from 60, E from 50, otherwise F. */
char student_grade(const Student *student);
int student_grade_index(char grade);

void student_format_roll(char *dst, size_t cap, uint32_t roll);
void student_format_tenths(char *dst, size_t cap, uint16_t tenths);

#endif
