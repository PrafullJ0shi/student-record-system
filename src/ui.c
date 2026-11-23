/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/ui.h"

#include "srms/stats.h"

#include <stdio.h>
#include <string.h>

#define UI_WIDTH 72
#define UI_STRIP 61

static void ink(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[22m\033[38;2;26;24;20m\033[48;2;243;240;232m", out);
    }
}

static void muted(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[22m\033[38;2;110;104;95m", out);
    }
}

static void accent(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[1m\033[38;2;184;67;26m", out);
    }
}

void ui_begin(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[48;2;243;240;232m\033[38;2;26;24;20m\033[4 q", out);
    }
}

void ui_end(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[0 q\033[0m", out);
    }
    fflush(out);
}

void ui_clear(FILE *out, const UiStyle *style)
{
    if (style->color) {
        fputs("\033[2J\033[H", out);
    }
}

static void rule(FILE *out, const UiStyle *style)
{
    int i;

    fputs("  ", out);
    muted(out, style);
    for (i = 0; i < UI_WIDTH; i++) {
        fputs(style->unicode ? "\xE2\x94\x80" : "-", out);
    }
    fputc('\n', out);
    ink(out, style);
}

static void banner(FILE *out, const UiStyle *style, const char *section)
{
    int left = (int)strlen(SRMS_VERSION) + 6;
    int gap = UI_WIDTH - left - (int)strlen(section);
    int i;

    if (gap < 1) {
        gap = 1;
    }
    fputs("  ", out);
    accent(out, style);
    fputs("SRMS", out);
    ink(out, style);
    fprintf(out, "  %s", SRMS_VERSION);
    for (i = 0; i < gap; i++) {
        fputc(' ', out);
    }
    muted(out, style);
    fputs(section, out);
    ink(out, style);
    fputc('\n', out);
}

static void fit_text(char *dst, size_t cap, const char *src, size_t width, int unicode)
{
    size_t length = strlen(src);

    if (length <= width) {
        snprintf(dst, cap, "%s", src);
        return;
    }
    if (unicode && width >= 2 && cap > width + 2u) {
        memcpy(dst, src, width - 1u);
        memcpy(dst + (width - 1u), "\xE2\x80\xA6", 3);
        dst[width - 1u + 3u] = '\0';
        return;
    }
    if (width >= 3 && cap > width) {
        memcpy(dst, src, width - 3u);
        memcpy(dst + (width - 3u), "...", 3);
        dst[width] = '\0';
        return;
    }
    if (cap > width) {
        memcpy(dst, src, width);
        dst[width] = '\0';
    } else if (cap > 0) {
        dst[0] = '\0';
    }
}

static void fit_path(char *dst, size_t cap, const char *path, size_t width)
{
    size_t length = strlen(path);
    size_t keep_end;
    size_t keep_start;

    if (length <= width) {
        snprintf(dst, cap, "%s", path);
        return;
    }
    if (width < 8 || cap <= width) {
        snprintf(dst, cap, "%s", path);
        return;
    }
    keep_end = width / 2u;
    keep_start = width - keep_end - 3u;
    snprintf(dst, cap, "%.*s...%s", (int)keep_start, path, path + (length - keep_end));
}

void ui_note(FILE *out, const UiStyle *style, const char *label, const char *text)
{
    fputs("\n  ", out);
    accent(out, style);
    fputs(label, out);
    ink(out, style);
    fprintf(out, "  %s\n", text);
    fflush(out);
}

static void field_row(FILE *out, const UiStyle *style, const char *key, const char *value)
{
    muted(out, style);
    fprintf(out, "  %-14s", key);
    ink(out, style);
    fprintf(out, "%s\n", value);
}

static void table_head(FILE *out, const UiStyle *style)
{
    muted(out, style);
    fprintf(out, "  %-6s  %-27s  %-16s  %4s  %5s  %s\n",
            "Roll", "Name", "Programme", "Year", "Pct", "Gr");
    ink(out, style);
    rule(out, style);
}

static void table_row(FILE *out, const UiStyle *style, const Student *student)
{
    char roll[16];
    char name[40];
    char programme[32];
    char pct[16];

    student_format_roll(roll, sizeof roll, student->roll);
    fit_text(name, sizeof name, student->name, 27, style->unicode);
    fit_text(programme, sizeof programme, student->programme, 16, style->unicode);
    student_format_tenths(pct, sizeof pct, student_percentage_tenths(student));
    fprintf(out, "  %-6s  %-27s  %-16s  %4u  %5s  %c\n",
            roll, name, programme, (unsigned)student->year, pct, student_grade(student));
}

void ui_render_menu(FILE *out, const UiStyle *style, const char *path,
                    size_t count, int dirty)
{
    static const struct {
        const char *num;
        const char *label;
        const char *gloss;
    } items[] = {
        {"1", "Add student", "assign the smallest free roll"},
        {"2", "Delete student", "remove a record by roll"},
        {"3", "View register", "table of every record"},
        {"4", "Modify student", "edit fields other than roll"},
        {"5", "Search", "roll, name, or programme"},
        {"6", "Sort register", "name, roll, or percentage"},
        {"7", "Statistics", "distribution, centre, ranking"},
        {"8", "Export CSV", "write a spreadsheet extract"},
        {"9", "Save", "write the versioned register"},
        {"0", "Quit", "leave this session"}
    };
    char shown[80];
    size_t i;

    banner(out, style, "Register");
    fputc('\n', out);
    fit_path(shown, sizeof shown, path, 56);
    field_row(out, style, "File", shown);
    snprintf(shown, sizeof shown, "%zu", count);
    field_row(out, style, "Records", shown);
    field_row(out, style, "State", dirty ? "unsaved" : "saved");
    fputc('\n', out);
    rule(out, style);
    for (i = 0; i < sizeof items / sizeof items[0]; i++) {
        fputs("  ", out);
        accent(out, style);
        fprintf(out, "%s", items[i].num);
        ink(out, style);
        fprintf(out, "   %-18s", items[i].label);
        muted(out, style);
        fprintf(out, "%s\n", items[i].gloss);
        ink(out, style);
    }
    fflush(out);
}

void ui_render_register(FILE *out, const UiStyle *style, const Registry *registry,
                        const char *section)
{
    const RegistryNode *node;

    banner(out, style, section);
    fputc('\n', out);
    if (registry->count == 0) {
        fprintf(out, "  The register is empty.\n");
        fprintf(out, "  Add a student to assign roll 0001.\n");
        fflush(out);
        return;
    }
    table_head(out, style);
    for (node = registry->head; node != NULL; node = node->next) {
        table_row(out, style, &node->student);
    }
    fprintf(out, "\n  %zu %s. The order is the current register order.\n",
            registry->count, registry->count == 1 ? "record" : "records");
    fflush(out);
}

void ui_render_matches(FILE *out, const UiStyle *style,
                       const Student *const *rows, size_t shown, size_t total)
{
    size_t i;

    banner(out, style, "Search");
    fputc('\n', out);
    if (total == 0) {
        fprintf(out, "  No records match.\n");
        fflush(out);
        return;
    }
    table_head(out, style);
    for (i = 0; i < shown; i++) {
        table_row(out, style, rows[i]);
    }
    if (shown == total) {
        fprintf(out, "\n  %zu %s.\n", total, total == 1 ? "match" : "matches");
    } else {
        fprintf(out, "\n  Showing %zu of %zu matches.\n", shown, total);
    }
    fflush(out);
}

void ui_render_detail(FILE *out, const UiStyle *style, const Student *student)
{
    char roll[16];
    char year[16];
    char mark[16];
    char pct[16];
    char grade[2];
    size_t i;

    student_format_roll(roll, sizeof roll, student->roll);
    snprintf(year, sizeof year, "%u", (unsigned)student->year);
    student_format_tenths(pct, sizeof pct, student_percentage_tenths(student));
    grade[0] = student_grade(student);
    grade[1] = '\0';
    fputc('\n', out);
    field_row(out, style, "Roll", roll);
    field_row(out, style, "Name", student->name);
    field_row(out, style, "Programme", student->programme);
    field_row(out, style, "Admitted", year);
    for (i = 0; i < SRMS_PAPER_COUNT; i++) {
        student_format_tenths(mark, sizeof mark, student->marks_tenths[i]);
        field_row(out, style, student_paper_name(i), mark);
    }
    field_row(out, style, "Percentage", pct);
    field_row(out, style, "Grade", grade);
    fflush(out);
}

static void bar_line(FILE *out, const UiStyle *style, const char *label,
                     size_t value, size_t scale)
{
    const int width = 28;
    int filled = 0;
    int i;
    (void)style;

    if (scale > 0 && value > 0) {
        filled = (int)((value * (size_t)width + scale / 2u) / scale);
        if (filled < 1) {
            filled = 1;
        }
        if (filled > width) {
            filled = width;
        }
    }
    fprintf(out, "  %-3s", label);
    for (i = 0; i < filled; i++) {
        fputc('#', out);
    }
    fprintf(out, "  %zu\n", value);
}

static void percentage_strip(FILE *out, const UiStyle *style, const Registry *registry)
{
    int bins[UI_STRIP];
    const RegistryNode *node;
    int i;
    char axis[UI_STRIP + 1];
    const char *dot = style->unicode ? "\xC2\xB7" : ".";

    memset(bins, 0, sizeof bins);
    memset(axis, ' ', (size_t)UI_STRIP);
    axis[UI_STRIP] = '\0';
    memcpy(axis + 0, "40", 2);
    memcpy(axis + 10, "50", 2);
    memcpy(axis + 20, "60", 2);
    memcpy(axis + 30, "70", 2);
    memcpy(axis + 40, "80", 2);
    memcpy(axis + 50, "90", 2);
    memcpy(axis + 57, "100", 3);

    for (node = registry->head; node != NULL; node = node->next) {
        uint16_t pct = student_percentage_tenths(&node->student);
        int column;
        if (pct <= 400u) {
            column = 0;
        } else if (pct >= 1000u) {
            column = UI_STRIP - 1;
        } else {
            column = (int)(((uint32_t)pct - 400u) * 60u / 600u);
        }
        if (column < 0) {
            column = 0;
        }
        if (column >= UI_STRIP) {
            column = UI_STRIP - 1;
        }
        bins[column]++;
    }

    fprintf(out, "  %s\n  ", axis);
    for (i = 0; i < UI_STRIP; i++) {
        if (bins[i] <= 0) {
            fputc(' ', out);
        } else if (bins[i] == 1) {
            fputs(dot, out);
        } else if (bins[i] < 10) {
            fprintf(out, "%d", bins[i]);
        } else {
            fputc('+', out);
        }
    }
    fputc('\n', out);
    muted(out, style);
    fprintf(out, "  Each mark is one student. The axis runs from 40 to 100.\n");
    ink(out, style);
}

void ui_render_stats(FILE *out, const UiStyle *style, const Registry *registry)
{
    ClassStats stats;
    size_t scale = 1;
    size_t i;
    static const char *labels[SRMS_GRADE_COUNT] = {"A", "B", "C", "D", "E", "F"};
    char line[96];
    char pct[16];
    char roll[16];
    char who[40];

    stats_compute(registry, &stats);
    banner(out, style, "Statistics");
    fputc('\n', out);
    if (stats.count == 0) {
        fprintf(out, "  No records. Statistics need at least one student.\n");
        fflush(out);
        return;
    }
    snprintf(line, sizeof line, "%zu", stats.count);
    field_row(out, style, "Students", line);
    student_format_tenths(pct, sizeof pct, (uint16_t)stats.average_tenths);
    field_row(out, style, "Average", pct);
    student_format_tenths(pct, sizeof pct, (uint16_t)stats.median_tenths);
    field_row(out, style, "Median", pct);
    if (stats.highest != NULL) {
        student_format_roll(roll, sizeof roll, stats.highest->roll);
        student_format_tenths(pct, sizeof pct, student_percentage_tenths(stats.highest));
        fit_text(who, sizeof who, stats.highest->name, 24, style->unicode);
        snprintf(line, sizeof line, "%s   %c   %s   roll %s",
                 pct, student_grade(stats.highest), who, roll);
        field_row(out, style, "Highest", line);
    }
    if (stats.lowest != NULL) {
        student_format_roll(roll, sizeof roll, stats.lowest->roll);
        student_format_tenths(pct, sizeof pct, student_percentage_tenths(stats.lowest));
        fit_text(who, sizeof who, stats.lowest->name, 24, style->unicode);
        snprintf(line, sizeof line, "%s   %c   %s   roll %s",
                 pct, student_grade(stats.lowest), who, roll);
        field_row(out, style, "Lowest", line);
    }

    fputc('\n', out);
    fputs("  ", out);
    accent(out, style);
    fputs("Fig. 1", out);
    ink(out, style);
    fprintf(out, "   Grade distribution\n");
    for (i = 0; i < SRMS_GRADE_COUNT; i++) {
        if (stats.grade_count[i] > scale) {
            scale = stats.grade_count[i];
        }
    }
    for (i = 0; i < SRMS_GRADE_COUNT; i++) {
        bar_line(out, style, labels[i], stats.grade_count[i], scale);
    }

    fputc('\n', out);
    fputs("  ", out);
    accent(out, style);
    fputs("Fig. 2", out);
    ink(out, style);
    fprintf(out, "   Percentage, low to high\n");
    percentage_strip(out, style, registry);

    fputc('\n', out);
    fprintf(out, "  Highest three\n");
    table_head(out, style);
    for (i = 0; i < stats.top_count; i++) {
        table_row(out, style, stats.top[i]);
    }
    fputc('\n', out);
    muted(out, style);
    fprintf(out, "  Grades   A from 90   B from 80   C from 70   D from 60   E from 50   F below 50\n");
    ink(out, style);
    fflush(out);
}
