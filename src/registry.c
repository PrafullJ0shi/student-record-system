/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/registry.h"

#include "internal.h"
#include "srms/validate.h"

#include <stdio.h>
#include <stdlib.h>

void registry_init(Registry *registry)
{
    registry->head = NULL;
    registry->tail = NULL;
    registry->count = 0;
}

void registry_clear(Registry *registry)
{
    RegistryNode *node = registry->head;

    while (node != NULL) {
        RegistryNode *next = node->next;
        free(node);
        node = next;
    }
    registry_init(registry);
}

static int cmp_primary(const Student *left, const Student *right, SrmsSortKey key)
{
    if (key == SRMS_SORT_ROLL) {
        if (left->roll < right->roll) {
            return -1;
        }
        if (left->roll > right->roll) {
            return 1;
        }
        return 0;
    }
    if (key == SRMS_SORT_NAME) {
        return srms_ascii_casecmp(left->name, right->name);
    }
    {
        uint16_t lp = student_percentage_tenths(left);
        uint16_t rp = student_percentage_tenths(right);
        if (lp < rp) {
            return -1;
        }
        if (lp > rp) {
            return 1;
        }
        return 0;
    }
}

static int cmp_order(const Student *left, const Student *right,
                     SrmsSortKey key, SrmsSortOrder order)
{
    int cmp = cmp_primary(left, right, key);
    if (order == SRMS_ORDER_DESC) {
        return -cmp;
    }
    return cmp;
}

SrmsStatus registry_insert(Registry *registry, const Student *student)
{
    RegistryNode *node;

    if (validate_student_fields(student, 1) != SRMS_OK) {
        return SRMS_ERR_VALIDATION;
    }
    if (registry->count >= SRMS_MAX_RECORDS) {
        srms_set_detail("The register holds at most %u records.", SRMS_MAX_RECORDS);
        return SRMS_ERR_RANGE;
    }
    if (registry_find_roll(registry, student->roll) != NULL) {
        srms_set_detail("Roll %u is already in the register.", (unsigned)student->roll);
        return SRMS_ERR_VALIDATION;
    }
    node = malloc(sizeof *node);
    if (node == NULL) {
        srms_set_detail("Out of memory.");
        return SRMS_ERR_NOMEM;
    }
    node->student = *student;
    node->next = NULL;
    if (registry->tail == NULL) {
        registry->head = node;
    } else {
        registry->tail->next = node;
    }
    registry->tail = node;
    registry->count++;
    return SRMS_OK;
}

uint32_t registry_smallest_free_roll(const Registry *registry)
{
    size_t limit;
    unsigned char *seen;
    const RegistryNode *node;
    uint32_t roll;

    /* The smallest missing positive integer is at most count + 1. */
    if (registry->count >= SRMS_ROLL_MAX) {
        return 0;
    }
    limit = registry->count + 1u;
    seen = calloc(limit + 1u, 1u);
    if (seen == NULL) {
        return 0;
    }
    for (node = registry->head; node != NULL; node = node->next) {
        if (node->student.roll <= limit) {
            seen[node->student.roll] = 1;
        }
    }
    for (roll = 1; roll <= limit; roll++) {
        if (!seen[roll]) {
            free(seen);
            return roll;
        }
    }
    free(seen);
    return 0;
}

SrmsStatus registry_add(Registry *registry, Student *student)
{
    uint32_t roll;
    SrmsStatus status;

    student->roll = 0;
    if (validate_student_fields(student, 0) != SRMS_OK) {
        return SRMS_ERR_VALIDATION;
    }
    roll = registry_smallest_free_roll(registry);
    if (roll == 0u) {
        srms_set_detail("A free roll number could not be assigned.");
        return SRMS_ERR_NOMEM;
    }
    student->roll = roll;
    status = registry_insert(registry, student);
    return status;
}

const Student *registry_find_roll(const Registry *registry, uint32_t roll)
{
    const RegistryNode *node;

    for (node = registry->head; node != NULL; node = node->next) {
        if (node->student.roll == roll) {
            return &node->student;
        }
    }
    return NULL;
}

SrmsStatus registry_remove(Registry *registry, uint32_t roll)
{
    RegistryNode *prev = NULL;
    RegistryNode *node;

    for (node = registry->head; node != NULL; node = node->next) {
        if (node->student.roll == roll) {
            if (prev == NULL) {
                registry->head = node->next;
            } else {
                prev->next = node->next;
            }
            if (registry->tail == node) {
                registry->tail = prev;
            }
            free(node);
            registry->count--;
            return SRMS_OK;
        }
        prev = node;
    }
    srms_set_detail("No record with roll %u.", (unsigned)roll);
    return SRMS_ERR_NOT_FOUND;
}

SrmsStatus registry_update(Registry *registry, const Student *student)
{
    RegistryNode *node;

    if (validate_student_fields(student, 1) != SRMS_OK) {
        return SRMS_ERR_VALIDATION;
    }
    for (node = registry->head; node != NULL; node = node->next) {
        if (node->student.roll == student->roll) {
            uint32_t roll = node->student.roll;
            node->student = *student;
            node->student.roll = roll;
            return SRMS_OK;
        }
    }
    srms_set_detail("No record with roll %u.", (unsigned)student->roll);
    return SRMS_ERR_NOT_FOUND;
}

static RegistryNode *merge_lists(RegistryNode *left, RegistryNode *right,
                                 SrmsSortKey key, SrmsSortOrder order)
{
    RegistryNode dummy;
    RegistryNode *tail = &dummy;

    dummy.next = NULL;
    while (left != NULL && right != NULL) {
        if (cmp_order(&left->student, &right->student, key, order) <= 0) {
            tail->next = left;
            left = left->next;
        } else {
            tail->next = right;
            right = right->next;
        }
        tail = tail->next;
    }
    tail->next = left != NULL ? left : right;
    return dummy.next;
}

static void split_list(RegistryNode *head, RegistryNode **left, RegistryNode **right)
{
    RegistryNode *slow = head;
    RegistryNode *fast = head->next;

    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
    }
    *left = head;
    *right = slow->next;
    slow->next = NULL;
}

static RegistryNode *merge_sort(RegistryNode *head, SrmsSortKey key, SrmsSortOrder order)
{
    RegistryNode *left;
    RegistryNode *right;

    if (head == NULL || head->next == NULL) {
        return head;
    }
    split_list(head, &left, &right);
    left = merge_sort(left, key, order);
    right = merge_sort(right, key, order);
    return merge_lists(left, right, key, order);
}

static void refresh_tail(Registry *registry)
{
    registry->tail = registry->head;
    if (registry->tail == NULL) {
        return;
    }
    while (registry->tail->next != NULL) {
        registry->tail = registry->tail->next;
    }
}

SrmsStatus registry_sort(Registry *registry, SrmsSortKey key, SrmsSortOrder order)
{
    if (key != SRMS_SORT_ROLL && key != SRMS_SORT_NAME && key != SRMS_SORT_PERCENTAGE) {
        srms_set_detail("Unknown sort key.");
        return SRMS_ERR_VALIDATION;
    }
    registry->head = merge_sort(registry->head, key, order);
    refresh_tail(registry);
    return SRMS_OK;
}

size_t registry_search(const Registry *registry, const char *query,
                       const Student **out, size_t cap)
{
    const RegistryNode *node;
    uint32_t roll = 0;
    int have_roll;
    size_t found = 0;

    if (query == NULL || query[0] == '\0') {
        return 0;
    }
    {
        char previous[512];
        snprintf(previous, sizeof previous, "%s", srms_detail());
        have_roll = validate_roll_text(query, &roll) == SRMS_OK;
        if (!have_roll) {
            srms_set_detail("%s", previous);
        }
    }
    for (node = registry->head; node != NULL; node = node->next) {
        int match = 0;
        if (have_roll && node->student.roll == roll) {
            match = 1;
        }
        if (srms_ascii_contains(node->student.name, query) ||
            srms_ascii_contains(node->student.programme, query)) {
            match = 1;
        }
        if (match) {
            if (out != NULL && found < cap) {
                out[found] = &node->student;
            }
            found++;
        }
    }
    return found;
}
