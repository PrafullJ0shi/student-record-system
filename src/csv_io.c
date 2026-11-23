/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/csv_io.h"

#include "internal.h"
#include "srms/validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_FIELD_CAP 128
#define CSV_MAX_COLUMNS 16

typedef struct CsvScan {
    const char *cursor;
    const char *end;
} CsvScan;

static SrmsStatus read_entire(const char *path, char **out, size_t *out_len)
{
    FILE *fp;
    ByteBuf buf;
    char chunk[4096];

    fp = fopen(path, "rb");
    if (fp == NULL) {
        srms_set_detail("Could not open %s.", path);
        return SRMS_ERR_IO;
    }
    bb_init(&buf);
    for (;;) {
        size_t n = fread(chunk, 1, sizeof chunk, fp);
        if (n > 0) {
            if (buf.len > SRMS_MAX_FILE_BYTES || n > SRMS_MAX_FILE_BYTES - buf.len) {
                bb_free(&buf);
                fclose(fp);
                srms_set_detail("The CSV file is larger than %u bytes.", SRMS_MAX_FILE_BYTES);
                return SRMS_ERR_RANGE;
            }
            if (bb_append(&buf, chunk, n) != SRMS_OK) {
                bb_free(&buf);
                fclose(fp);
                return SRMS_ERR_NOMEM;
            }
        }
        if (n < sizeof chunk) {
            if (ferror(fp)) {
                bb_free(&buf);
                fclose(fp);
                srms_set_detail("Could not read %s.", path);
                return SRMS_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);
    *out = buf.data;
    *out_len = buf.len;
    return SRMS_OK;
}

static int csv_field(CsvScan *scan, char *dst, size_t cap, int *end_of_row)
{
    size_t n = 0;

    *end_of_row = 0;
    if (scan->cursor >= scan->end) {
        return 0;
    }
    if (*scan->cursor == '"') {
        int closed = 0;
        scan->cursor++;
        while (scan->cursor < scan->end) {
            char ch = *scan->cursor++;
            if (ch == '"') {
                if (scan->cursor < scan->end && *scan->cursor == '"') {
                    scan->cursor++;
                } else {
                    closed = 1;
                    break;
                }
            }
            if (n + 1u >= cap) {
                srms_set_detail("A CSV field is longer than %u characters.", (unsigned)(cap - 1u));
                return -1;
            }
            dst[n++] = ch;
        }
        if (!closed) {
            srms_set_detail("A quoted CSV field is missing its closing quote.");
            return -1;
        }
    } else {
        while (scan->cursor < scan->end && *scan->cursor != ',' &&
               *scan->cursor != '\n' && *scan->cursor != '\r') {
            if (n + 1u >= cap) {
                srms_set_detail("A CSV field is longer than %u characters.", (unsigned)(cap - 1u));
                return -1;
            }
            dst[n++] = *scan->cursor++;
        }
    }
    dst[n] = '\0';
    if (scan->cursor < scan->end && *scan->cursor == ',') {
        scan->cursor++;
        return 1;
    }
    if (scan->cursor < scan->end && *scan->cursor == '\r') {
        scan->cursor++;
    }
    if (scan->cursor < scan->end && *scan->cursor == '\n') {
        scan->cursor++;
    }
    *end_of_row = 1;
    return 1;
}

static SrmsStatus append_field(ByteBuf *buf, const char *text, int last)
{
    const char *p;
    int special = 0;
    SrmsStatus status;

    for (p = text; *p != '\0'; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            special = 1;
            break;
        }
    }
    if (!special) {
        status = bb_append(buf, text, strlen(text));
    } else {
        status = bb_append(buf, "\"", 1);
        for (p = text; status == SRMS_OK && *p != '\0'; p++) {
            if (*p == '"') {
                status = bb_append(buf, "\"\"", 2);
            } else {
                status = bb_append(buf, p, 1);
            }
        }
        if (status == SRMS_OK) {
            status = bb_append(buf, "\"", 1);
        }
    }
    if (status != SRMS_OK) {
        return status;
    }
    return bb_append(buf, last ? "\n" : ",", 1);
}

SrmsStatus csv_export(const Registry *registry, const char *path)
{
    ByteBuf buf;
    const RegistryNode *node;
    SrmsStatus status;
    char roll[16];
    char year[16];
    char mark[16];
    char pct[16];
    char grade[2];
    size_t paper;

    bb_init(&buf);
    status = bb_append(&buf,
                       "roll,name,programme,year,paper_1,paper_2,paper_3,paper_4,practical,percentage,grade\n",
                       strlen("roll,name,programme,year,paper_1,paper_2,paper_3,paper_4,practical,percentage,grade\n"));
    for (node = registry->head; status == SRMS_OK && node != NULL; node = node->next) {
        const Student *student = &node->student;
        student_format_roll(roll, sizeof roll, student->roll);
        snprintf(year, sizeof year, "%u", (unsigned)student->year);
        student_format_tenths(pct, sizeof pct, student_percentage_tenths(student));
        grade[0] = student_grade(student);
        grade[1] = '\0';
        status = append_field(&buf, roll, 0);
        if (status == SRMS_OK) {
            status = append_field(&buf, student->name, 0);
        }
        if (status == SRMS_OK) {
            status = append_field(&buf, student->programme, 0);
        }
        if (status == SRMS_OK) {
            status = append_field(&buf, year, 0);
        }
        for (paper = 0; status == SRMS_OK && paper < SRMS_PAPER_COUNT; paper++) {
            student_format_tenths(mark, sizeof mark, student->marks_tenths[paper]);
            status = append_field(&buf, mark, 0);
        }
        if (status == SRMS_OK) {
            status = append_field(&buf, pct, 0);
        }
        if (status == SRMS_OK) {
            status = append_field(&buf, grade, 1);
        }
    }
    if (status == SRMS_OK) {
        status = atomic_write(path, buf.data == NULL ? "" : buf.data, buf.len);
    }
    bb_free(&buf);
    return status;
}

static int column_id(const char *name)
{
    if (strcmp(name, "roll") == 0) {
        return 0;
    }
    if (strcmp(name, "name") == 0) {
        return 1;
    }
    if (strcmp(name, "programme") == 0) {
        return 2;
    }
    if (strcmp(name, "year") == 0) {
        return 3;
    }
    if (strcmp(name, "paper_1") == 0) {
        return 4;
    }
    if (strcmp(name, "paper_2") == 0) {
        return 5;
    }
    if (strcmp(name, "paper_3") == 0) {
        return 6;
    }
    if (strcmp(name, "paper_4") == 0) {
        return 7;
    }
    if (strcmp(name, "practical") == 0) {
        return 8;
    }
    if (strcmp(name, "percentage") == 0 || strcmp(name, "grade") == 0) {
        return 9;
    }
    return -2;
}

SrmsStatus csv_import(Registry *registry, const char *path)
{
    char *file = NULL;
    size_t length = 0;
    CsvScan scan;
    Registry temp;
    int map[10];
    SrmsStatus status;
    int end_of_row = 0;
    int saw_header = 0;
    size_t row_number = 0;
    size_t i;

    status = read_entire(path, &file, &length);
    if (status != SRMS_OK) {
        return status;
    }
    if (file == NULL) {
        srms_set_detail("The CSV file is empty.");
        return SRMS_ERR_FORMAT;
    }
    if (length >= 3u && (unsigned char)file[0] == 0xEFu &&
        (unsigned char)file[1] == 0xBBu && (unsigned char)file[2] == 0xBFu) {
        free(file);
        srms_set_detail("The CSV file begins with a byte-order mark. Save it as plain UTF-8.");
        return SRMS_ERR_FORMAT;
    }
    if (memchr(file, '\0', length) != NULL) {
        free(file);
        srms_set_detail("The CSV file contains a NUL byte.");
        return SRMS_ERR_FORMAT;
    }

    for (i = 0; i < 10u; i++) {
        map[i] = -1;
    }
    scan.cursor = file;
    scan.end = file + length;
    registry_init(&temp);

    while (scan.cursor < scan.end) {
        char fields[CSV_MAX_COLUMNS][CSV_FIELD_CAP];
        int count = 0;
        Student student;

        end_of_row = 0;
        while (!end_of_row) {
            int got;
            if (count >= CSV_MAX_COLUMNS) {
                free(file);
                registry_clear(&temp);
                srms_set_detail("Row %zu has too many columns.", row_number + 1u);
                return SRMS_ERR_FORMAT;
            }
            got = csv_field(&scan, fields[count], CSV_FIELD_CAP, &end_of_row);
            if (got < 0) {
                free(file);
                registry_clear(&temp);
                return SRMS_ERR_FORMAT;
            }
            if (got == 0) {
                break;
            }
            count++;
        }
        if (count == 0) {
            break;
        }
        if (count == 1 && fields[0][0] == '\0') {
            continue;
        }
        row_number++;
        if (!saw_header) {
            saw_header = 1;
            for (i = 0; i < (size_t)count; i++) {
                int id = column_id(fields[i]);
                if (id == -2) {
                    free(file);
                    registry_clear(&temp);
                    srms_set_detail("Unknown CSV column \"%s\".", fields[i]);
                    return SRMS_ERR_FORMAT;
                }
                if (id >= 0 && id < 9 && map[id] >= 0) {
                    free(file);
                    registry_clear(&temp);
                    srms_set_detail("CSV column \"%s\" is repeated.", fields[i]);
                    return SRMS_ERR_FORMAT;
                }
                if (id >= 0 && id < 9) {
                    map[id] = (int)i;
                }
            }
            for (i = 1; i <= 8u; i++) {
                if (map[i] < 0) {
                    free(file);
                    registry_clear(&temp);
                    srms_set_detail("The CSV header is missing a required column.");
                    return SRMS_ERR_FORMAT;
                }
            }
            continue;
        }

        memset(&student, 0, sizeof student);
        if (map[0] >= 0) {
            uint32_t roll = 0;
            if (map[0] >= count || validate_roll_text(fields[map[0]], &roll) != SRMS_OK) {
                free(file);
                registry_clear(&temp);
                srms_set_detail("Row %zu has a roll that is not valid.", row_number);
                return SRMS_ERR_FORMAT;
            }
            student.roll = roll;
        }
        if (map[1] >= count || map[2] >= count || map[3] >= count) {
            free(file);
            registry_clear(&temp);
            srms_set_detail("Row %zu is missing a field.", row_number);
            return SRMS_ERR_FORMAT;
        }
        if (strlen(fields[map[1]]) >= SRMS_NAME_CAP || strlen(fields[map[2]]) >= SRMS_PROGRAMME_CAP) {
            free(file);
            registry_clear(&temp);
            srms_set_detail("Row %zu has a name or programme that is too long.", row_number);
            return SRMS_ERR_FORMAT;
        }
        memcpy(student.name, fields[map[1]], strlen(fields[map[1]]) + 1u);
        memcpy(student.programme, fields[map[2]], strlen(fields[map[2]]) + 1u);
        if (validate_year(fields[map[3]], &student.year) != SRMS_OK) {
            free(file);
            registry_clear(&temp);
            srms_set_detail("Row %zu has a year that is not valid.", row_number);
            return SRMS_ERR_FORMAT;
        }
        for (i = 0; i < SRMS_PAPER_COUNT; i++) {
            int col = map[4 + i];
            if (col < 0 || col >= count ||
                validate_mark(fields[col], &student.marks_tenths[i]) != SRMS_OK) {
                free(file);
                registry_clear(&temp);
                srms_set_detail("Row %zu has a mark that is not valid.", row_number);
                return SRMS_ERR_FORMAT;
            }
        }
        if (student.roll == 0u) {
            status = registry_add(&temp, &student);
        } else {
            status = registry_insert(&temp, &student);
        }
        if (status != SRMS_OK) {
            char previous[512];
            snprintf(previous, sizeof previous, "%s", srms_detail());
            srms_set_detail("Row %zu: %s", row_number, previous);
            free(file);
            registry_clear(&temp);
            return status;
        }
    }
    free(file);
    if (!saw_header) {
        registry_clear(&temp);
        srms_set_detail("The CSV file is empty.");
        return SRMS_ERR_FORMAT;
    }
    registry_clear(registry);
    *registry = temp;
    return SRMS_OK;
}
