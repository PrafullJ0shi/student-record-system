/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "internal.h"

#include <ctype.h>
#include <string.h>

int srms_ascii_casecmp(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        int a = tolower((unsigned char)*left);
        int b = tolower((unsigned char)*right);
        if (a != b) {
            return a - b;
        }
        left++;
        right++;
    }
    return tolower((unsigned char)*left) - tolower((unsigned char)*right);
}

int srms_ascii_contains(const char *haystack, const char *needle)
{
    size_t nlen;
    size_t hlen;
    size_t i;

    if (needle == NULL || needle[0] == '\0') {
        return 0;
    }
    nlen = strlen(needle);
    hlen = strlen(haystack);
    if (nlen > hlen) {
        return 0;
    }
    for (i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        while (j < nlen) {
            unsigned char a = (unsigned char)haystack[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (tolower(a) != tolower(b)) {
                break;
            }
            j++;
        }
        if (j == nlen) {
            return 1;
        }
    }
    return 0;
}
