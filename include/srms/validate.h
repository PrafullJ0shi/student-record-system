/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_VALIDATE_H
#define SRMS_VALIDATE_H

#include "srms/status.h"
#include "srms/student.h"

#include <stddef.h>
#include <stdint.h>

SrmsStatus validate_name(const char *text);
SrmsStatus validate_programme(const char *text);
SrmsStatus validate_year(const char *text, uint16_t *year);
SrmsStatus validate_mark(const char *text, uint16_t *tenths);
SrmsStatus validate_roll_text(const char *text, uint32_t *roll);

/* Roll may be 0 when it has not been assigned yet. */
SrmsStatus validate_student_fields(const Student *student, int roll_required);

#endif
