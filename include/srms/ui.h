/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_UI_H
#define SRMS_UI_H

#include "srms/registry.h"

#include <stdio.h>

typedef struct UiStyle {
    int color;
    int unicode;
} UiStyle;

void ui_begin(FILE *out, const UiStyle *style);
void ui_end(FILE *out, const UiStyle *style);
void ui_clear(FILE *out, const UiStyle *style);

void ui_render_menu(FILE *out, const UiStyle *style, const char *path,
                    size_t count, int dirty);
void ui_render_register(FILE *out, const UiStyle *style, const Registry *registry,
                        const char *section);
void ui_render_matches(FILE *out, const UiStyle *style,
                       const Student *const *rows, size_t shown, size_t total);
void ui_render_detail(FILE *out, const UiStyle *style, const Student *student);
void ui_render_stats(FILE *out, const UiStyle *style, const Registry *registry);
void ui_note(FILE *out, const UiStyle *style, const char *label, const char *text);

#endif
