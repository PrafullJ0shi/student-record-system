/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/validate.h"

#include <ctype.h>
#include <string.h>

static int is_name_char(unsigned char c)
{
    return isalpha(c) || c == ' ' || c == '-' || c == '\'' || c == '.';
}

static SrmsStatus check_label(const char *text, size_t max_chars, const char *what)
{
    size_t n;
    size_t i;
    int prev_space = 0;

    if (text == NULL) {
        srms_set_detail("%s is missing.", what);
        return SRMS_ERR_VALIDATION;
    }
    n = strlen(text);
    if (n == 0 || n > max_chars) {
        srms_set_detail("%s must be 1 to %u characters.", what, (unsigned)max_chars);
        return SRMS_ERR_VALIDATION;
    }
    if (text[0] == ' ' || text[n - 1] == ' ') {
        srms_set_detail("%s must not begin or end with a space.", what);
        return SRMS_ERR_VALIDATION;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (!is_name_char(c)) {
            srms_set_detail("%s contains a character that is not allowed.", what);
            return SRMS_ERR_VALIDATION;
        }
        if (c == ' ') {
            if (prev_space) {
                srms_set_detail("%s contains a double space.", what);
                return SRMS_ERR_VALIDATION;
            }
            prev_space = 1;
        } else {
            prev_space = 0;
        }
    }
    return SRMS_OK;
}

SrmsStatus validate_name(const char *text)
{
    return check_label(text, SRMS_NAME_CAP - 1u, "Name");
}

SrmsStatus validate_programme(const char *text)
{
    return check_label(text, SRMS_PROGRAMME_CAP - 1u, "Programme");
}

static int parse_u32(const char *text, uint32_t *out, uint32_t max_value)
{
    uint32_t value = 0;
    const char *p = text;

    size_t digits = 0;

    if (p == NULL || *p == '\0') {
        return 0;
    }
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        digits++;
        if (digits > 12u) {
            return 0;
        }
        if (value > (max_value - (uint32_t)(*p - '0')) / 10u) {
            return 0;
        }
        value = value * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *out = value;
    return 1;
}

SrmsStatus validate_year(const char *text, uint16_t *year)
{
    uint32_t value = 0;

    if (!parse_u32(text, &value, SRMS_YEAR_MAX)) {
        srms_set_detail("Year must be a whole number from %d to %d.",
                        SRMS_YEAR_MIN, SRMS_YEAR_MAX);
        return SRMS_ERR_VALIDATION;
    }
    if (value < SRMS_YEAR_MIN || value > SRMS_YEAR_MAX) {
        srms_set_detail("Year must be from %d to %d.", SRMS_YEAR_MIN, SRMS_YEAR_MAX);
        return SRMS_ERR_VALIDATION;
    }
    *year = (uint16_t)value;
    return SRMS_OK;
}

SrmsStatus validate_mark(const char *text, uint16_t *tenths)
{
    uint32_t whole = 0;
    uint32_t frac = 0;
    const char *p;
    int saw_digit = 0;

    if (text == NULL || text[0] == '\0') {
        srms_set_detail("A mark is required.");
        return SRMS_ERR_VALIDATION;
    }
    p = text;
    while (*p >= '0' && *p <= '9') {
        saw_digit = 1;
        if (whole > (1000u - (uint32_t)(*p - '0')) / 10u) {
            srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
            return SRMS_ERR_VALIDATION;
        }
        whole = whole * 10u + (uint32_t)(*p - '0');
        p++;
    }
    if (!saw_digit) {
        srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
        return SRMS_ERR_VALIDATION;
    }
    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9') {
            srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
            return SRMS_ERR_VALIDATION;
        }
        frac = (uint32_t)(*p - '0');
        p++;
        if (*p != '\0') {
            srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
            return SRMS_ERR_VALIDATION;
        }
    } else if (*p != '\0') {
        srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
        return SRMS_ERR_VALIDATION;
    }

    if (whole > 100u || (whole == 100u && frac != 0u)) {
        srms_set_detail("A mark must be from 0 to 100, with at most one decimal.");
        return SRMS_ERR_VALIDATION;
    }
    *tenths = (uint16_t)(whole * 10u + frac);
    return SRMS_OK;
}

SrmsStatus validate_roll_text(const char *text, uint32_t *roll)
{
    uint32_t value = 0;

    if (!parse_u32(text, &value, SRMS_ROLL_MAX) || value == 0u) {
        srms_set_detail("Roll must be a whole number from 1 to %u.", SRMS_ROLL_MAX);
        return SRMS_ERR_VALIDATION;
    }
    *roll = value;
    return SRMS_OK;
}

SrmsStatus validate_student_fields(const Student *student, int roll_required)
{
    size_t i;

    if (student == NULL) {
        srms_set_detail("The record is missing.");
        return SRMS_ERR_VALIDATION;
    }
    if (roll_required || student->roll != 0u) {
        if (student->roll == 0u || student->roll > SRMS_ROLL_MAX) {
            srms_set_detail("Roll must be a whole number from 1 to %u.", SRMS_ROLL_MAX);
            return SRMS_ERR_VALIDATION;
        }
    }
    if (validate_name(student->name) != SRMS_OK) {
        return SRMS_ERR_VALIDATION;
    }
    if (validate_programme(student->programme) != SRMS_OK) {
        return SRMS_ERR_VALIDATION;
    }
    if (student->year < SRMS_YEAR_MIN || student->year > SRMS_YEAR_MAX) {
        srms_set_detail("Year must be from %d to %d.", SRMS_YEAR_MIN, SRMS_YEAR_MAX);
        return SRMS_ERR_VALIDATION;
    }
    for (i = 0; i < SRMS_PAPER_COUNT; i++) {
        if (student->marks_tenths[i] > SRMS_MARK_TENTHS_MAX) {
            srms_set_detail("%s must be from 0 to 100.", student_paper_name(i));
            return SRMS_ERR_VALIDATION;
        }
    }
    return SRMS_OK;
}
