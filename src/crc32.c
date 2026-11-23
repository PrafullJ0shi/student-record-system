/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/crc32.h"

uint32_t srms_crc32(const void *data, size_t length)
{
    const unsigned char *bytes = data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    for (i = 0; i < length; i++) {
        int bit;
        crc ^= bytes[i];
        for (bit = 0; bit < 8; bit++) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}
