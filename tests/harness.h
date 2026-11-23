/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_TEST_HARNESS_H
#define SRMS_TEST_HARNESS_H

void srms_check(int condition, const char *file, int line, const char *expr);
int srms_check_failures(void);
int srms_check_passes(void);

#define CHECK(cond) srms_check(!!(cond), __FILE__, __LINE__, #cond)

#endif
