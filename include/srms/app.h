/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#ifndef SRMS_APP_H
#define SRMS_APP_H

#include "srms/ui.h"

#include <stdio.h>

typedef struct AppConfig {
    const char *path;
    UiStyle style;
    int confirm_quit;
} AppConfig;

/* Interactive session. Returns a process exit code. */
int app_run(FILE *in, FILE *out, const AppConfig *config);

int app_import_csv(const char *register_path, const char *csv_path, FILE *err);
int app_export_csv(const char *register_path, const char *csv_path, FILE *err);
int app_show_stats(const char *register_path, FILE *out, FILE *err, const UiStyle *style);
int app_write_report(const char *register_path, const char *report_path, FILE *err);

#endif
