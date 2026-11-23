/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "srms/app.h"
#include "srms/student.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(FILE *out)
{
    fputs(
        "srms — student record management\n"
        "\n"
        "Usage\n"
        "  srms [--file PATH] [--color | --no-color] [--ascii]\n"
        "  srms --file PATH --import-csv FILE\n"
        "  srms --file PATH --export-csv FILE\n"
        "  srms --file PATH --stats [--color | --no-color] [--ascii]\n"
        "  srms --file PATH --report FILE\n"
        "\n"
        "The default register is students.srms in the working directory.\n"
        "Colour follows the terminal unless NO_COLOR is set. --color forces it.\n"
        "--ascii draws rules and charts with ASCII instead of Unicode.\n"
        "A report file of \"-\" writes the machine-readable summary to stdout.\n",
        out);
}

int main(int argc, char **argv)
{
    const char *path = "students.srms";
    const char *import_csv = NULL;
    const char *export_csv = NULL;
    const char *report_path = NULL;
    int show_stats = 0;
    int color_flag = -1;
    int ascii = 0;
    int actions = 0;
    int i;
    AppConfig config;
    int tty;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("srms %s\n", SRMS_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error  --file needs a path.\n");
                return 1;
            }
            path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--import-csv") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error  --import-csv needs a path.\n");
                return 1;
            }
            import_csv = argv[++i];
            actions++;
            continue;
        }
        if (strcmp(argv[i], "--export-csv") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error  --export-csv needs a path.\n");
                return 1;
            }
            export_csv = argv[++i];
            actions++;
            continue;
        }
        if (strcmp(argv[i], "--report") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error  --report needs a path.\n");
                return 1;
            }
            report_path = argv[++i];
            actions++;
            continue;
        }
        if (strcmp(argv[i], "--stats") == 0) {
            show_stats = 1;
            actions++;
            continue;
        }
        if (strcmp(argv[i], "--color") == 0) {
            color_flag = 1;
            continue;
        }
        if (strcmp(argv[i], "--no-color") == 0) {
            color_flag = 0;
            continue;
        }
        if (strcmp(argv[i], "--ascii") == 0) {
            ascii = 1;
            continue;
        }
        fprintf(stderr, "Error  Unknown argument %s.\n", argv[i]);
        usage(stderr);
        return 1;
    }

    if (actions > 1) {
        fprintf(stderr, "Error  Use only one of --import-csv, --export-csv, --stats, or --report.\n");
        return 1;
    }
    if (import_csv != NULL) {
        return app_import_csv(path, import_csv, stderr);
    }
    if (export_csv != NULL) {
        return app_export_csv(path, export_csv, stderr);
    }
    if (report_path != NULL) {
        return app_write_report(path, report_path, stderr);
    }

    tty = isatty(fileno(stdin)) && isatty(fileno(stdout));
    memset(&config, 0, sizeof config);
    config.path = path;
    config.style.unicode = ascii ? 0 : 1;
    if (color_flag >= 0) {
        config.style.color = color_flag;
    } else {
        config.style.color = tty && getenv("NO_COLOR") == NULL;
    }
    config.confirm_quit = isatty(fileno(stdin));

    if (show_stats) {
        return app_show_stats(path, stdout, stderr, &config.style);
    }
    return app_run(stdin, stdout, &config);
}
