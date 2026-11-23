/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_REGISTRY_H
#define SRMS_REGISTRY_H

#include "srms/status.h"
#include "srms/student.h"

#include <stddef.h>
#include <stdint.h>

typedef struct RegistryNode {
    Student student;
    struct RegistryNode *next;
} RegistryNode;

typedef struct Registry {
    RegistryNode *head;
    RegistryNode *tail;
    size_t count;
} Registry;

typedef enum SrmsSortKey {
    SRMS_SORT_ROLL = 0,
    SRMS_SORT_NAME = 1,
    SRMS_SORT_PERCENTAGE = 2
} SrmsSortKey;

typedef enum SrmsSortOrder {
    SRMS_ORDER_ASC = 0,
    SRMS_ORDER_DESC = 1
} SrmsSortOrder;

void registry_init(Registry *registry);
void registry_clear(Registry *registry);

/* Copies the record. Roll must already be set, in range, and unique. */
SrmsStatus registry_insert(Registry *registry, const Student *student);

/* Validates fields, assigns the smallest free roll, then inserts. */
SrmsStatus registry_add(Registry *registry, Student *student);

SrmsStatus registry_remove(Registry *registry, uint32_t roll);

/* Replaces every field of the matching roll except the roll itself. */
SrmsStatus registry_update(Registry *registry, const Student *student);

const Student *registry_find_roll(const Registry *registry, uint32_t roll);

/* Smallest positive roll not in use. Returns 0 if the value cannot be found. */
uint32_t registry_smallest_free_roll(const Registry *registry);

/* Stable merge sort. Equal keys keep their previous order. */
SrmsStatus registry_sort(Registry *registry, SrmsSortKey key, SrmsSortOrder order);

/* Substring match on name and programme, plus an exact roll if the query
 * parses as one. Writes up to cap pointers. Returns the total number of hits.
 */
size_t registry_search(const Registry *registry, const char *query,
                       const Student **out, size_t cap);

#endif
