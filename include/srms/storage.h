/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_STORAGE_H
#define SRMS_STORAGE_H

#include "srms/registry.h"
#include "srms/status.h"

/* On failure the destination registry is left unchanged. */
SrmsStatus storage_load(Registry *registry, const char *path);
SrmsStatus storage_save(const Registry *registry, const char *path);

#endif
