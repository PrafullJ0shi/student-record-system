/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_CSV_IO_H
#define SRMS_CSV_IO_H

#include "srms/registry.h"
#include "srms/status.h"

SrmsStatus csv_export(const Registry *registry, const char *path);

/* Replaces the registry only after the whole file has been accepted.
 * A roll column, when present, is kept. Otherwise rolls are assigned
 * in file order as the smallest free number.
 */
SrmsStatus csv_import(Registry *registry, const char *path);

#endif
