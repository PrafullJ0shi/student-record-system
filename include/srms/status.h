/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_STATUS_H
#define SRMS_STATUS_H

typedef enum SrmsStatus {
    SRMS_OK = 0,
    SRMS_ERR_IO = 1,
    SRMS_ERR_FORMAT = 2,
    SRMS_ERR_CHECKSUM = 3,
    SRMS_ERR_VALIDATION = 4,
    SRMS_ERR_NOT_FOUND = 5,
    SRMS_ERR_NOMEM = 6,
    SRMS_ERR_RANGE = 7,
    SRMS_ERR_CANCELLED = 8
} SrmsStatus;

const char *srms_status_string(SrmsStatus status);

void srms_clear_detail(void);
void srms_set_detail(const char *fmt, ...);
const char *srms_detail(void);

#endif
