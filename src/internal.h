/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_INTERNAL_H
#define SRMS_INTERNAL_H

#include "srms/status.h"

#include <stddef.h>
#include <stdint.h>

int srms_ascii_casecmp(const char *left, const char *right);
int srms_ascii_contains(const char *haystack, const char *needle);

typedef struct ByteBuf {
    char *data;
    size_t len;
    size_t cap;
} ByteBuf;

void bb_init(ByteBuf *buf);
void bb_free(ByteBuf *buf);
SrmsStatus bb_append(ByteBuf *buf, const char *data, size_t n);
SrmsStatus bb_printf(ByteBuf *buf, const char *fmt, ...);

SrmsStatus atomic_write(const char *path, const void *data, size_t len);

#endif
