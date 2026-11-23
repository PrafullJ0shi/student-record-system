/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/status.h"

#include <stdarg.h>
#include <stdio.h>

static char g_detail[512];

const char *srms_status_string(SrmsStatus status)
{
    switch (status) {
    case SRMS_OK:
        return "ok";
    case SRMS_ERR_IO:
        return "input or output failed";
    case SRMS_ERR_FORMAT:
        return "register format is not valid";
    case SRMS_ERR_CHECKSUM:
        return "register checksum does not match";
    case SRMS_ERR_VALIDATION:
        return "a field is not valid";
    case SRMS_ERR_NOT_FOUND:
        return "no record with that roll";
    case SRMS_ERR_NOMEM:
        return "out of memory";
    case SRMS_ERR_RANGE:
        return "a value is outside the supported range";
    case SRMS_ERR_CANCELLED:
        return "cancelled";
    }
    return "unknown status";
}

void srms_clear_detail(void)
{
    g_detail[0] = '\0';
}

void srms_set_detail(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    vsnprintf(g_detail, sizeof g_detail, fmt, ap);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    va_end(ap);
}

const char *srms_detail(void)
{
    return g_detail;
}
