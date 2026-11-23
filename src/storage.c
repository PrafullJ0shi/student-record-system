/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/storage.h"

#include "internal.h"
#include "srms/crc32.h"
#include "srms/validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SrmsStatus read_header_line(FILE *fp, char *buf, size_t cap)
{
    size_t n;

    if (fgets(buf, (int)cap, fp) == NULL) {
        srms_set_detail("The register header is incomplete.");
        return SRMS_ERR_FORMAT;
    }
    n = strlen(buf);
    if (n == 0 || buf[n - 1] != '\n') {
        srms_set_detail("A header line is missing its newline or is too long.");
        return SRMS_ERR_FORMAT;
    }
    buf[n - 1] = '\0';
    if (n >= 2 && buf[n - 2] == '\r') {
        buf[n - 2] = '\0';
    }
    return SRMS_OK;
}

static int parse_hex32(const char *text, uint32_t *out)
{
    uint32_t value = 0;
    int i;

    if (strlen(text) != 8u) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        unsigned char c = (unsigned char)text[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (uint32_t)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
        value = (value << 4) | digit;
    }
    *out = value;
    return 1;
}

static int split_fields(char *line, char **fields, int expect)
{
    int count = 0;
    char *start = line;
    char *p = line;

    for (;;) {
        if (*p == '|' || *p == '\0') {
            int finished = (*p == '\0');
            if (count >= expect) {
                return 0;
            }
            *p = '\0';
            fields[count++] = start;
            if (finished) {
                break;
            }
            start = p + 1;
        }
        p++;
    }
    return count == expect;
}

static int parse_tenths_field(const char *text, uint16_t *out)
{
    uint32_t value = 0;
    const char *p = text;

    if (p == NULL || *p == '\0') {
        return 0;
    }
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        if (value > (SRMS_MARK_TENTHS_MAX - (uint32_t)(*p - '0')) / 10u) {
            return 0;
        }
        value = value * 10u + (uint32_t)(*p - '0');
        p++;
    }
    if (value > SRMS_MARK_TENTHS_MAX) {
        return 0;
    }
    *out = (uint16_t)value;
    return 1;
}

static SrmsStatus parse_record(char *line, Student *student, size_t record_index)
{
    char *fields[9];
    size_t i;
    uint32_t roll = 0;
    uint16_t year = 0;

    if (!split_fields(line, fields, 9)) {
        srms_set_detail("Record %zu does not have 9 fields.", record_index);
        return SRMS_ERR_FORMAT;
    }
    memset(student, 0, sizeof *student);
    if (validate_roll_text(fields[0], &roll) != SRMS_OK) {
        srms_set_detail("Record %zu has a roll that is not valid.", record_index);
        return SRMS_ERR_FORMAT;
    }
    student->roll = roll;
    if (strlen(fields[1]) >= SRMS_NAME_CAP || strlen(fields[2]) >= SRMS_PROGRAMME_CAP) {
        srms_set_detail("Record %zu has a name or programme that is too long.", record_index);
        return SRMS_ERR_FORMAT;
    }
    memcpy(student->name, fields[1], strlen(fields[1]) + 1u);
    memcpy(student->programme, fields[2], strlen(fields[2]) + 1u);
    if (validate_year(fields[3], &year) != SRMS_OK) {
        srms_set_detail("Record %zu has a year that is not valid.", record_index);
        return SRMS_ERR_FORMAT;
    }
    student->year = year;
    for (i = 0; i < SRMS_PAPER_COUNT; i++) {
        if (!parse_tenths_field(fields[4 + i], &student->marks_tenths[i])) {
            srms_set_detail("Record %zu has a mark that is not valid.", record_index);
            return SRMS_ERR_FORMAT;
        }
    }
    if (validate_student_fields(student, 1) != SRMS_OK) {
        char previous[512];
        snprintf(previous, sizeof previous, "%s", srms_detail());
        srms_set_detail("Record %zu: %s", record_index, previous);
        return SRMS_ERR_FORMAT;
    }
    return SRMS_OK;
}

static SrmsStatus read_payload(FILE *fp, char **out, size_t *out_len)
{
    ByteBuf buf;
    char chunk[4096];

    bb_init(&buf);
    for (;;) {
        size_t n = fread(chunk, 1, sizeof chunk, fp);
        if (n > 0) {
            if (buf.len > SRMS_MAX_FILE_BYTES || n > SRMS_MAX_FILE_BYTES - buf.len) {
                bb_free(&buf);
                srms_set_detail("The register is larger than %u bytes.", SRMS_MAX_FILE_BYTES);
                return SRMS_ERR_RANGE;
            }
            if (bb_append(&buf, chunk, n) != SRMS_OK) {
                bb_free(&buf);
                return SRMS_ERR_NOMEM;
            }
        }
        if (n < sizeof chunk) {
            if (ferror(fp)) {
                bb_free(&buf);
                srms_set_detail("Could not read the register.");
                return SRMS_ERR_IO;
            }
            break;
        }
    }
    *out = buf.data;
    *out_len = buf.len;
    return SRMS_OK;
}

static SrmsStatus load_into(Registry *registry, const char *path)
{
    FILE *fp;
    char line[128];
    unsigned long version = 0;
    unsigned long declared = 0;
    uint32_t expected_crc = 0;
    char *payload = NULL;
    size_t payload_len = 0;
    uint32_t actual_crc;
    size_t offset = 0;
    size_t seen = 0;
    char *end = NULL;
    SrmsStatus status = SRMS_OK;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        srms_set_detail("Could not open %s.", path);
        return SRMS_ERR_IO;
    }
    status = read_header_line(fp, line, sizeof line);
    if (status != SRMS_OK) {
        fclose(fp);
        return status;
    }
    if (strncmp(line, "SRMS ", 5) != 0) {
        fclose(fp);
        srms_set_detail("The file does not begin with an SRMS header.");
        return SRMS_ERR_FORMAT;
    }
    if (line[5] < '0' || line[5] > '9') {
        fclose(fp);
        srms_set_detail("This build reads register version %d.", SRMS_FORMAT_VERSION);
        return SRMS_ERR_FORMAT;
    }
    version = strtoul(line + 5, &end, 10);
    if (end == line + 5 || *end != '\0' || version != SRMS_FORMAT_VERSION) {
        fclose(fp);
        srms_set_detail("This build reads register version %d.", SRMS_FORMAT_VERSION);
        return SRMS_ERR_FORMAT;
    }
    status = read_header_line(fp, line, sizeof line);
    if (status != SRMS_OK) {
        fclose(fp);
        return status;
    }
    if (strncmp(line, "count ", 6) != 0) {
        fclose(fp);
        srms_set_detail("The count line is missing.");
        return SRMS_ERR_FORMAT;
    }
    if (line[6] < '0' || line[6] > '9') {
        fclose(fp);
        srms_set_detail("The record count is missing or too large.");
        return SRMS_ERR_FORMAT;
    }
    declared = strtoul(line + 6, &end, 10);
    if (end == line + 6 || *end != '\0' || declared > SRMS_MAX_RECORDS) {
        fclose(fp);
        srms_set_detail("The record count is missing or too large.");
        return SRMS_ERR_FORMAT;
    }
    status = read_header_line(fp, line, sizeof line);
    if (status != SRMS_OK) {
        fclose(fp);
        return status;
    }
    if (strncmp(line, "crc32 ", 6) != 0 || !parse_hex32(line + 6, &expected_crc)) {
        fclose(fp);
        srms_set_detail("The checksum line is missing or not 8 hex digits.");
        return SRMS_ERR_FORMAT;
    }
    status = read_header_line(fp, line, sizeof line);
    if (status != SRMS_OK) {
        fclose(fp);
        return status;
    }
    if (strcmp(line, "---") != 0) {
        fclose(fp);
        srms_set_detail("The header separator is missing.");
        return SRMS_ERR_FORMAT;
    }
    status = read_payload(fp, &payload, &payload_len);
    fclose(fp);
    if (status != SRMS_OK) {
        return status;
    }
    actual_crc = srms_crc32(payload == NULL ? "" : payload, payload_len);
    if (actual_crc != expected_crc) {
        free(payload);
        srms_set_detail("Checksum mismatch. The file was edited or truncated.");
        return SRMS_ERR_CHECKSUM;
    }
    if (payload == NULL) {
        payload = calloc(1, 1);
        if (payload == NULL) {
            return SRMS_ERR_NOMEM;
        }
        payload_len = 0;
    }
    while (offset < payload_len) {
        size_t start = offset;
        Student student;
        while (offset < payload_len && payload[offset] != '\n') {
            offset++;
        }
        if (offset >= payload_len) {
            free(payload);
            srms_set_detail("The last record does not end with a newline.");
            return SRMS_ERR_FORMAT;
        }
        payload[offset] = '\0';
        if (offset > start && payload[offset - 1] == '\r') {
            payload[offset - 1] = '\0';
        }
        offset++;
        seen++;
        if (payload[start] == '\0') {
            free(payload);
            srms_set_detail("Record %zu is blank.", seen);
            return SRMS_ERR_FORMAT;
        }
        status = parse_record(payload + start, &student, seen);
        if (status != SRMS_OK) {
            free(payload);
            return status;
        }
        status = registry_insert(registry, &student);
        if (status != SRMS_OK) {
            char previous[512];
            snprintf(previous, sizeof previous, "%s", srms_detail());
            srms_set_detail("Record %zu: %s", seen, previous);
            free(payload);
            return status;
        }
    }
    free(payload);
    if (seen != declared) {
        srms_set_detail("The header says %lu records but the file contains %zu.",
                        declared, seen);
        return SRMS_ERR_FORMAT;
    }
    return SRMS_OK;
}

SrmsStatus storage_load(Registry *registry, const char *path)
{
    Registry temp;
    SrmsStatus status;

    registry_init(&temp);
    status = load_into(&temp, path);
    if (status != SRMS_OK) {
        registry_clear(&temp);
        return status;
    }
    registry_clear(registry);
    *registry = temp;
    return SRMS_OK;
}

SrmsStatus storage_save(const Registry *registry, const char *path)
{
    ByteBuf payload;
    ByteBuf file;
    const RegistryNode *node;
    uint32_t crc;
    SrmsStatus status;

    bb_init(&payload);
    bb_init(&file);
    for (node = registry->head; node != NULL; node = node->next) {
        const Student *student = &node->student;
        status = bb_printf(&payload,
                           "%u|%s|%s|%u|%u|%u|%u|%u|%u\n",
                           (unsigned)student->roll,
                           student->name,
                           student->programme,
                           (unsigned)student->year,
                           (unsigned)student->marks_tenths[0],
                           (unsigned)student->marks_tenths[1],
                           (unsigned)student->marks_tenths[2],
                           (unsigned)student->marks_tenths[3],
                           (unsigned)student->marks_tenths[4]);
        if (status != SRMS_OK) {
            bb_free(&payload);
            return status;
        }
    }
    crc = srms_crc32(payload.data == NULL ? "" : payload.data, payload.len);
    status = bb_printf(&file, "SRMS %d\ncount %zu\ncrc32 %08x\n---\n",
                       SRMS_FORMAT_VERSION, registry->count, (unsigned)crc);
    if (status == SRMS_OK && payload.len > 0) {
        status = bb_append(&file, payload.data, payload.len);
    }
    bb_free(&payload);
    if (status != SRMS_OK) {
        bb_free(&file);
        return status;
    }
    status = atomic_write(path, file.data == NULL ? "" : file.data, file.len);
    bb_free(&file);
    return status;
}
