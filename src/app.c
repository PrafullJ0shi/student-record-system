/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/app.h"

#include "srms/csv_io.h"
#include "srms/stats.h"
#include "srms/storage.h"
#include "srms/validate.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void show_status(FILE *out, const UiStyle *style, SrmsStatus status)
{
    const char *detail = srms_detail();

    if (detail[0] == '\0') {
        detail = srms_status_string(status);
    }
    ui_note(out, style, "Error", detail);
}

static char *trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '\0') {
        return text;
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
        *end = '\0';
    }
    return end > text ? text : text;
}

static SrmsStatus read_line(FILE *in, char *buf, size_t cap)
{
    size_t n;
    int extra;

    if (fgets(buf, (int)cap, in) == NULL) {
        buf[0] = '\0';
        return SRMS_ERR_IO;
    }
    n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') {
        buf[--n] = '\0';
        if (n > 0 && buf[n - 1] == '\r') {
            buf[--n] = '\0';
        }
        return SRMS_OK;
    }
    extra = fgetc(in);
    if (extra == '\n' || extra == EOF) {
        return SRMS_OK;
    }
    while (extra != EOF && extra != '\n') {
        extra = fgetc(in);
    }
    buf[0] = '\0';
    srms_set_detail("That line is too long.");
    return SRMS_ERR_VALIDATION;
}

static void prompt_label(FILE *out, const UiStyle *style, const char *label)
{
    fputc('\n', out);
    if (style->color) {
        fputs("\033[38;2;110;104;95m", out);
    }
    fprintf(out, "  %s\n", label);
    if (style->color) {
        fputs("\033[38;2;26;24;20m", out);
    }
    fputs("  > ", out);
    fflush(out);
}

static int prompt_text(FILE *in, FILE *out, const UiStyle *style,
                       const char *label, char *buf, size_t cap)
{
    SrmsStatus status;

    prompt_label(out, style, label);
    status = read_line(in, buf, cap);
    if (status == SRMS_ERR_IO) {
        return -1;
    }
    if (status != SRMS_OK) {
        show_status(out, style, status);
        return 0;
    }
    /* A terminal echoes the newline. A pipe does not, so move off the prompt. */
    if (!isatty(fileno(in))) {
        fputc('\n', out);
    }
    return 1;
}

static int confirm_answer(FILE *in, FILE *out, const UiStyle *style, const char *label)
{
    char buf[64];
    char *text;

    for (;;) {
        int got = prompt_text(in, out, style, label, buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        text = trim(buf);
        if (strcmp(text, "y") == 0 || strcmp(text, "Y") == 0 ||
            strcmp(text, "yes") == 0 || strcmp(text, "Yes") == 0) {
            return 1;
        }
        if (text[0] == '\0' || strcmp(text, "n") == 0 || strcmp(text, "N") == 0 ||
            strcmp(text, "no") == 0 || strcmp(text, "No") == 0) {
            return 0;
        }
        ui_note(out, style, "Error", "Answer y or n.");
    }
}

static int pause_enter(FILE *in, FILE *out, const UiStyle *style)
{
    char buf[32];
    int got = prompt_text(in, out, style, "Press enter", buf, sizeof buf);
    return got < 0 ? -1 : 0;
}

static void default_csv_path(char *dst, size_t cap, const char *register_path)
{
    const char *slash = strrchr(register_path, '/');
    char *dot;

    snprintf(dst, cap, "%s", register_path);
    dot = strrchr(dst, '.');
    if (dot != NULL && (slash == NULL || dot > dst + (slash - register_path))) {
        snprintf(dot, cap - (size_t)(dot - dst), ".csv");
        return;
    }
    if (strlen(dst) + 4u < cap) {
        memcpy(dst + strlen(dst), ".csv", 5);
    }
}

static int save_registry(FILE *out, const UiStyle *style, Registry *registry,
                         const char *path, int *dirty)
{
    SrmsStatus status = storage_save(registry, path);
    char message[512];

    if (status != SRMS_OK) {
        show_status(out, style, status);
        return -1;
    }
    *dirty = 0;
    snprintf(message, sizeof message, "%zu %s to %s",
             registry->count, registry->count == 1 ? "record" : "records", path);
    ui_note(out, style, "Wrote", message);
    return 0;
}

static int do_add(FILE *in, FILE *out, const UiStyle *style, Registry *registry, int *dirty)
{
    Student student;
    char buf[128];
    char *text;
    size_t paper;
    int answer;
    static const char *mark_labels[SRMS_PAPER_COUNT] = {
        "Paper I  (0-100)",
        "Paper II  (0-100)",
        "Paper III  (0-100)",
        "Paper IV  (0-100)",
        "Practical  (0-100)"
    };

    memset(&student, 0, sizeof student);
    ui_clear(out, style);
    fprintf(out, "  Add student\n");
    fprintf(out, "  The roll is assigned when you confirm. A blank name cancels.\n");

    for (;;) {
        int got = prompt_text(in, out, style, "Name", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        text = trim(buf);
        if (text[0] == '\0') {
            ui_note(out, style, "Cancelled", "No record was added.");
            return 0;
        }
        if (validate_name(text) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        snprintf(student.name, sizeof student.name, "%s", text);
        break;
    }
    for (;;) {
        int got = prompt_text(in, out, style, "Programme", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        text = trim(buf);
        if (validate_programme(text) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        snprintf(student.programme, sizeof student.programme, "%s", text);
        break;
    }
    for (;;) {
        int got = prompt_text(in, out, style, "Year of admission", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (validate_year(trim(buf), &student.year) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        break;
    }
    for (paper = 0; paper < SRMS_PAPER_COUNT; paper++) {
        for (;;) {
            int got = prompt_text(in, out, style, mark_labels[paper], buf, sizeof buf);
            if (got < 0) {
                return -1;
            }
            if (got == 0) {
                continue;
            }
            if (validate_mark(trim(buf), &student.marks_tenths[paper]) != SRMS_OK) {
                show_status(out, style, SRMS_ERR_VALIDATION);
                continue;
            }
            break;
        }
    }
    if (registry_add(registry, &student) != SRMS_OK) {
        show_status(out, style, SRMS_ERR_VALIDATION);
        return 0;
    }
    ui_render_detail(out, style, &student);
    answer = confirm_answer(in, out, style, "Keep this record? [y/n]");
    if (answer < 0) {
        registry_remove(registry, student.roll);
        return -1;
    }
    if (answer == 0) {
        registry_remove(registry, student.roll);
        ui_note(out, style, "Cancelled", "No record was added.");
        return 0;
    }
    *dirty = 1;
    ui_note(out, style, "Added", "The record is in this session. Save to write it.");
    return pause_enter(in, out, style);
}

static int do_delete(FILE *in, FILE *out, const UiStyle *style, Registry *registry, int *dirty)
{
    char buf[64];
    uint32_t roll = 0;
    const Student *student;
    int answer;

    ui_clear(out, style);
    fprintf(out, "  Delete student\n");
    for (;;) {
        int got = prompt_text(in, out, style, "Roll", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (trim(buf)[0] == '\0') {
            ui_note(out, style, "Cancelled", "Nothing was deleted.");
            return 0;
        }
        if (validate_roll_text(trim(buf), &roll) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        break;
    }
    student = registry_find_roll(registry, roll);
    if (student == NULL) {
        srms_set_detail("No record with roll %u.", (unsigned)roll);
        show_status(out, style, SRMS_ERR_NOT_FOUND);
        return 0;
    }
    ui_render_detail(out, style, student);
    answer = confirm_answer(in, out, style, "Delete this record? [y/n]");
    if (answer <= 0) {
        if (answer == 0) {
            ui_note(out, style, "Cancelled", "Nothing was deleted.");
        }
        return answer < 0 ? -1 : 0;
    }
    if (registry_remove(registry, roll) != SRMS_OK) {
        show_status(out, style, SRMS_ERR_NOT_FOUND);
        return 0;
    }
    *dirty = 1;
    ui_note(out, style, "Deleted", "The roll can be assigned again. Save to write the register.");
    return pause_enter(in, out, style);
}

static int edit_text_field(FILE *in, FILE *out, const UiStyle *style, const char *label,
                           char *dst, size_t cap, SrmsStatus (*check)(const char *))
{
    char buf[128];

    for (;;) {
        int got = prompt_text(in, out, style, label, buf, sizeof buf);
        char *text;
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        text = trim(buf);
        if (text[0] == '\0') {
            return 0;
        }
        if (check(text) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        snprintf(dst, cap, "%s", text);
        return 1;
    }
}

static int do_modify(FILE *in, FILE *out, const UiStyle *style, Registry *registry, int *dirty)
{
    char buf[128];
    uint32_t roll = 0;
    Student student;
    const Student *found;

    ui_clear(out, style);
    fprintf(out, "  Modify student\n");
    fprintf(out, "  Roll numbers stay fixed.\n");
    for (;;) {
        int got = prompt_text(in, out, style, "Roll", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (trim(buf)[0] == '\0') {
            return 0;
        }
        if (validate_roll_text(trim(buf), &roll) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            continue;
        }
        break;
    }
    found = registry_find_roll(registry, roll);
    if (found == NULL) {
        srms_set_detail("No record with roll %u.", (unsigned)roll);
        show_status(out, style, SRMS_ERR_NOT_FOUND);
        return 0;
    }
    student = *found;
    for (;;) {
        int got;
        ui_clear(out, style);
        fprintf(out, "  Modify student\n");
        ui_render_detail(out, style, &student);
        fprintf(out, "\n  1  Name\n  2  Programme\n  3  Year of admission\n");
        fprintf(out, "  4  Paper I\n  5  Paper II\n  6  Paper III\n  7  Paper IV\n  8  Practical\n");
        fprintf(out, "  0  Back\n");
        got = prompt_text(in, out, style, "Field", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        {
        char *field = trim(buf);
        if (strcmp(field, "0") == 0) {
            return 0;
        }
        if (strcmp(field, "1") == 0) {
            int edited = edit_text_field(in, out, style, "Name", student.name,
                                         sizeof student.name, validate_name);
            if (edited < 0) {
                return -1;
            }
            if (edited == 0) {
                continue;
            }
        } else if (strcmp(field, "2") == 0) {
            int edited = edit_text_field(in, out, style, "Programme", student.programme,
                                         sizeof student.programme, validate_programme);
            if (edited < 0) {
                return -1;
            }
            if (edited == 0) {
                continue;
            }
        } else if (strcmp(field, "3") == 0) {
            int year_got = prompt_text(in, out, style, "Year of admission", buf, sizeof buf);
            if (year_got < 0) {
                return -1;
            }
            if (year_got == 0 || trim(buf)[0] == '\0') {
                continue;
            }
            if (validate_year(trim(buf), &student.year) != SRMS_OK) {
                show_status(out, style, SRMS_ERR_VALIDATION);
                continue;
            }
        } else if (field[0] >= '4' && field[0] <= '8' && field[1] == '\0') {
            size_t paper = (size_t)(field[0] - '4');
            int mark_got = prompt_text(in, out, style, student_paper_name(paper), buf, sizeof buf);
            if (mark_got < 0) {
                return -1;
            }
            if (mark_got == 0 || trim(buf)[0] == '\0') {
                continue;
            }
            if (validate_mark(trim(buf), &student.marks_tenths[paper]) != SRMS_OK) {
                show_status(out, style, SRMS_ERR_VALIDATION);
                continue;
            }
        } else {
            ui_note(out, style, "Error", "Choose a field number from the list.");
            continue;
        }
        if (registry_update(registry, &student) != SRMS_OK) {
            show_status(out, style, SRMS_ERR_VALIDATION);
            found = registry_find_roll(registry, roll);
            if (found != NULL) {
                student = *found;
            }
            continue;
        }
        *dirty = 1;
        found = registry_find_roll(registry, roll);
        if (found != NULL) {
            student = *found;
        }
        }
    }
}

static int do_search(FILE *in, FILE *out, const UiStyle *style, const Registry *registry)
{
    char buf[128];
    const Student *hits[64];
    size_t total;
    size_t shown;

    ui_clear(out, style);
    fprintf(out, "  Search\n");
    fprintf(out, "  A number matches a roll. Other text matches name or programme.\n");
    for (;;) {
        int got = prompt_text(in, out, style, "Query", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (trim(buf)[0] == '\0') {
            ui_note(out, style, "Cancelled", "No query was run.");
            return 0;
        }
        break;
    }
    total = registry_search(registry, trim(buf), hits, 64);
    shown = total < 64u ? total : 64u;
    ui_clear(out, style);
    ui_render_matches(out, style, hits, shown, total);
    return pause_enter(in, out, style);
}

static int do_sort(FILE *in, FILE *out, const UiStyle *style, Registry *registry, int *dirty)
{
    char buf[64];
    SrmsSortKey key;
    SrmsSortOrder order;
    const char *key_name;

    ui_clear(out, style);
    fprintf(out, "  Sort register\n");
    fprintf(out, "  Equal keys keep their previous order. Save to write the new order.\n");
    for (;;) {
        int got = prompt_text(in, out, style, "Key    1 roll    2 name    3 percentage", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (strcmp(trim(buf), "1") == 0) {
            key = SRMS_SORT_ROLL;
            key_name = "roll";
            break;
        }
        if (strcmp(trim(buf), "2") == 0) {
            key = SRMS_SORT_NAME;
            key_name = "name";
            break;
        }
        if (strcmp(trim(buf), "3") == 0) {
            key = SRMS_SORT_PERCENTAGE;
            key_name = "percentage";
            break;
        }
        ui_note(out, style, "Error", "Choose 1, 2, or 3.");
    }
    for (;;) {
        int got = prompt_text(in, out, style, "Order  1 ascending    2 descending", buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        if (strcmp(trim(buf), "1") == 0) {
            order = SRMS_ORDER_ASC;
            break;
        }
        if (strcmp(trim(buf), "2") == 0) {
            order = SRMS_ORDER_DESC;
            break;
        }
        ui_note(out, style, "Error", "Choose 1 or 2.");
    }
    if (registry_sort(registry, key, order) != SRMS_OK) {
        show_status(out, style, SRMS_ERR_VALIDATION);
        return 0;
    }
    *dirty = 1;
    {
        char message[128];
        snprintf(message, sizeof message, "%zu records by %s, %s.",
                 registry->count, key_name,
                 order == SRMS_ORDER_ASC ? "ascending" : "descending");
        ui_note(out, style, "Sorted", message);
    }
    return pause_enter(in, out, style);
}

static int do_export(FILE *in, FILE *out, const UiStyle *style, const Registry *registry,
                     const char *register_path)
{
    char suggestion[512];
    char buf[512];
    char label[640];
    const char *path;
    SrmsStatus status;
    char message[640];

    default_csv_path(suggestion, sizeof suggestion, register_path);
    ui_clear(out, style);
    fprintf(out, "  Export CSV\n");
    snprintf(label, sizeof label, "Path  [%s]", suggestion);
    for (;;) {
        int got = prompt_text(in, out, style, label, buf, sizeof buf);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            continue;
        }
        path = trim(buf);
        if (path[0] == '\0') {
            path = suggestion;
        }
        break;
    }
    status = csv_export(registry, path);
    if (status != SRMS_OK) {
        show_status(out, style, status);
        return 0;
    }
    snprintf(message, sizeof message, "%zu rows to %s", registry->count, path);
    ui_note(out, style, "Exported", message);
    return pause_enter(in, out, style);
}

static int finish_quit(FILE *in, FILE *out, const UiStyle *style, Registry *registry,
                       const AppConfig *config, int *dirty)
{
    if (*dirty && config->confirm_quit) {
        int answer = confirm_answer(in, out, style, "Save changes before quitting? [y/n]");
        if (answer < 0) {
            return -1;
        }
        if (answer == 1 && save_registry(out, style, registry, config->path, dirty) != 0) {
            return 1;
        }
    }
    return 0;
}

int app_run(FILE *in, FILE *out, const AppConfig *config)
{
    Registry registry;
    int dirty = 0;
    int exit_code = 0;

    registry_init(&registry);
    ui_begin(out, &config->style);
    if (access(config->path, F_OK) == 0) {
        SrmsStatus status = storage_load(&registry, config->path);
        if (status != SRMS_OK) {
            show_status(out, &config->style, status);
            ui_end(out, &config->style);
            registry_clear(&registry);
            return 2;
        }
    }

    for (;;) {
        char buf[64];
        int got;
        const char *choice;

        ui_clear(out, &config->style);
        ui_render_menu(out, &config->style, config->path, registry.count, dirty);
        got = prompt_text(in, out, &config->style, "Select", buf, sizeof buf);
        if (got < 0) {
            if (finish_quit(in, out, &config->style, &registry, config, &dirty) == 1) {
                continue;
            }
            break;
        }
        if (got == 0) {
            continue;
        }
        choice = trim(buf);
        if (strcmp(choice, "0") == 0) {
            int quit = finish_quit(in, out, &config->style, &registry, config, &dirty);
            if (quit == 1) {
                continue;
            }
            if (quit < 0) {
                break;
            }
            break;
        } else if (strcmp(choice, "1") == 0) {
            if (do_add(in, out, &config->style, &registry, &dirty) < 0) {
                break;
            }
        } else if (strcmp(choice, "2") == 0) {
            if (do_delete(in, out, &config->style, &registry, &dirty) < 0) {
                break;
            }
        } else if (strcmp(choice, "3") == 0) {
            ui_clear(out, &config->style);
            ui_render_register(out, &config->style, &registry, "Register");
            if (pause_enter(in, out, &config->style) < 0) {
                break;
            }
        } else if (strcmp(choice, "4") == 0) {
            if (do_modify(in, out, &config->style, &registry, &dirty) < 0) {
                break;
            }
        } else if (strcmp(choice, "5") == 0) {
            if (do_search(in, out, &config->style, &registry) < 0) {
                break;
            }
        } else if (strcmp(choice, "6") == 0) {
            if (do_sort(in, out, &config->style, &registry, &dirty) < 0) {
                break;
            }
        } else if (strcmp(choice, "7") == 0) {
            ui_clear(out, &config->style);
            ui_render_stats(out, &config->style, &registry);
            if (pause_enter(in, out, &config->style) < 0) {
                break;
            }
        } else if (strcmp(choice, "8") == 0) {
            if (do_export(in, out, &config->style, &registry, config->path) < 0) {
                break;
            }
        } else if (strcmp(choice, "9") == 0) {
            save_registry(out, &config->style, &registry, config->path, &dirty);
            if (pause_enter(in, out, &config->style) < 0) {
                break;
            }
        } else {
            ui_note(out, &config->style, "Error", "Choose a number from the list.");
            if (pause_enter(in, out, &config->style) < 0) {
                break;
            }
        }
    }

    ui_end(out, &config->style);
    registry_clear(&registry);
    return exit_code;
}

static int load_or_fail(Registry *registry, const char *path, FILE *err)
{
    SrmsStatus status;

    registry_init(registry);
    if (access(path, F_OK) != 0) {
        fprintf(err, "Error  No register at %s.\n", path);
        return 2;
    }
    status = storage_load(registry, path);
    if (status != SRMS_OK) {
        const char *detail = srms_detail();
        if (detail[0] == '\0') {
            detail = srms_status_string(status);
        }
        fprintf(err, "Error  %s\n", detail);
        registry_clear(registry);
        return 2;
    }
    return 0;
}

int app_import_csv(const char *register_path, const char *csv_path, FILE *err)
{
    Registry registry;
    SrmsStatus status;

    registry_init(&registry);
    status = csv_import(&registry, csv_path);
    if (status != SRMS_OK) {
        const char *detail = srms_detail();
        fprintf(err, "Error  %s\n", detail[0] ? detail : srms_status_string(status));
        registry_clear(&registry);
        return 2;
    }
    status = storage_save(&registry, register_path);
    if (status != SRMS_OK) {
        const char *detail = srms_detail();
        fprintf(err, "Error  %s\n", detail[0] ? detail : srms_status_string(status));
        registry_clear(&registry);
        return 2;
    }
    printf("Imported %zu records into %s\n", registry.count, register_path);
    registry_clear(&registry);
    return 0;
}

int app_export_csv(const char *register_path, const char *csv_path, FILE *err)
{
    Registry registry;
    int code = load_or_fail(&registry, register_path, err);
    SrmsStatus status;

    if (code != 0) {
        return code;
    }
    status = csv_export(&registry, csv_path);
    if (status != SRMS_OK) {
        const char *detail = srms_detail();
        fprintf(err, "Error  %s\n", detail[0] ? detail : srms_status_string(status));
        registry_clear(&registry);
        return 2;
    }
    printf("Exported %zu rows to %s\n", registry.count, csv_path);
    registry_clear(&registry);
    return 0;
}

int app_show_stats(const char *register_path, FILE *out, FILE *err, const UiStyle *style)
{
    Registry registry;
    int code = load_or_fail(&registry, register_path, err);

    if (code != 0) {
        return code;
    }
    ui_begin(out, style);
    ui_render_stats(out, style, &registry);
    ui_end(out, style);
    registry_clear(&registry);
    return 0;
}

int app_write_report(const char *register_path, const char *report_path, FILE *err)
{
    Registry registry;
    ClassStats stats;
    int code = load_or_fail(&registry, register_path, err);
    FILE *out;

    if (code != 0) {
        return code;
    }
    stats_compute(&registry, &stats);
    if (strcmp(report_path, "-") == 0) {
        stats_write_report(stdout, &registry, &stats);
        registry_clear(&registry);
        return 0;
    }
    out = fopen(report_path, "wb");
    if (out == NULL) {
        fprintf(err, "Error  Could not open %s.\n", report_path);
        registry_clear(&registry);
        return 2;
    }
    stats_write_report(out, &registry, &stats);
    if (fclose(out) != 0) {
        fprintf(err, "Error  Could not close %s.\n", report_path);
        registry_clear(&registry);
        return 2;
    }
    registry_clear(&registry);
    return 0;
}
