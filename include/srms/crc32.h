/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_CRC32_H
#define SRMS_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* ISO-HDLC CRC-32: polynomial 0xEDB88320, init and xorout 0xFFFFFFFF. */
uint32_t srms_crc32(const void *data, size_t length);

#endif
