/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Prafull Joshi
 */
#include "harness.h"

#include "srms/app.h"
#include "srms/crc32.h"
#include "srms/csv_io.h"
#include "srms/registry.h"
#include "srms/stats.h"
#include "srms/storage.h"
#include "srms/ui.h"
#include "srms/validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Student make_student(uint32_t roll, const char *name, const char *programme,
                            uint16_t year, uint16_t mark)
{
    Student student;
    size_t i;

    memset(&student, 0, sizeof student);
    student.roll = roll;
    snprintf(student.name, sizeof student.name, "%s", name);
    snprintf(student.programme, sizeof student.programme, "%s", programme);
    student.year = year;
    for (i = 0; i < SRMS_PAPER_COUNT; i++) {
        student.marks_tenths[i] = mark;
    }
    return student;
}

static char *make_dir(void)
{
    char *tmpl = malloc(64);
    if (tmpl == NULL) {
        return NULL;
    }
    snprintf(tmpl, 64, "/tmp/srms-test-XXXXXX");
    if (mkdtemp(tmpl) == NULL) {
        free(tmpl);
        return NULL;
    }
    return tmpl;
}

static void path_join(char *dst, size_t cap, const char *dir, const char *name)
{
    snprintf(dst, cap, "%s/%s", dir, name);
}

static void test_crc(void)
{
    const char *vector = "123456789";
    uint32_t crc = srms_crc32(vector, strlen(vector));
    CHECK(crc == 0xCBF43926u);
    CHECK(srms_crc32("", 0) == 0u);
}

static void test_student_math(void)
{
    Student student = make_student(1, "Ann", "Physics", 2020, 964);
    Student low;
    size_t i;

    CHECK(student_percentage_tenths(&student) == 964u);
    CHECK(student_grade(&student) == 'A');

    memset(&low, 0, sizeof low);
    low.marks_tenths[0] = 1;
    low.marks_tenths[1] = 1;
    low.marks_tenths[2] = 1;
    low.marks_tenths[3] = 1;
    low.marks_tenths[4] = 2;
    CHECK(student_percentage_tenths(&low) == 1u);

    student = make_student(1, "Ann", "Physics", 2020, 800);
    CHECK(student_grade(&student) == 'B');
    student = make_student(1, "Ann", "Physics", 2020, 700);
    CHECK(student_grade(&student) == 'C');
    student = make_student(1, "Ann", "Physics", 2020, 600);
    CHECK(student_grade(&student) == 'D');
    student = make_student(1, "Ann", "Physics", 2020, 500);
    CHECK(student_grade(&student) == 'E');
    student = make_student(1, "Ann", "Physics", 2020, 499);
    CHECK(student_grade(&student) == 'F');
    CHECK(student_grade_index('C') == 2);
    CHECK(student_grade_index('Z') == -1);
    CHECK(strcmp(student_paper_name(4), "Practical") == 0);
    CHECK(student_paper_name(9)[0] == '\0');

    for (i = 0; i < 5u; i++) {
        low.marks_tenths[i] = 1000;
    }
    CHECK(student_percentage_tenths(&low) == 1000u);
}

static void test_validate(void)
{
    uint16_t mark = 0;
    uint16_t year = 0;
    uint32_t roll = 0;

    CHECK(validate_name("Meera Iyer") == SRMS_OK);
    CHECK(validate_name("Mary-Jane O'Neil") == SRMS_OK);
    CHECK(validate_name("") != SRMS_OK);
    CHECK(validate_name("  Ann") != SRMS_OK);
    CHECK(validate_name("Ann  Bee") != SRMS_OK);
    CHECK(validate_name("Ann2") != SRMS_OK);
    CHECK(validate_programme("Physics") == SRMS_OK);
    CHECK(validate_programme("This Programme Name Is Too Long") != SRMS_OK);
    CHECK(validate_year("2022", &year) == SRMS_OK && year == 2022);
    CHECK(validate_year("1969", &year) != SRMS_OK);
    CHECK(validate_year("2101", &year) != SRMS_OK);
    CHECK(validate_year("20ab", &year) != SRMS_OK);
    CHECK(validate_mark("0", &mark) == SRMS_OK && mark == 0);
    CHECK(validate_mark("100", &mark) == SRMS_OK && mark == 1000);
    CHECK(validate_mark("84.2", &mark) == SRMS_OK && mark == 842);
    CHECK(validate_mark("100.0", &mark) == SRMS_OK && mark == 1000);
    CHECK(validate_mark("100.1", &mark) != SRMS_OK);
    CHECK(validate_mark("84.25", &mark) != SRMS_OK);
    CHECK(validate_mark("-1", &mark) != SRMS_OK);
    CHECK(validate_mark("84.", &mark) != SRMS_OK);
    CHECK(validate_roll_text("0007", &roll) == SRMS_OK && roll == 7u);
    CHECK(validate_roll_text("0", &roll) != SRMS_OK);
    CHECK(validate_roll_text("1000000", &roll) != SRMS_OK);
}

static void test_registry(void)
{
    Registry registry;
    Student ann = make_student(0, "Ann", "Physics", 2020, 700);
    Student bea = make_student(0, "Bea", "Mathematics", 2021, 800);
    Student cara = make_student(0, "Cara", "Physics", 2022, 700);
    const Student *hits[4];
    const RegistryNode *node;
    size_t i;

    registry_init(&registry);
    CHECK(registry_smallest_free_roll(&registry) == 1u);
    CHECK(registry_add(&registry, &ann) == SRMS_OK);
    CHECK(ann.roll == 1u);
    CHECK(registry_add(&registry, &bea) == SRMS_OK);
    CHECK(bea.roll == 2u);
    CHECK(registry_add(&registry, &cara) == SRMS_OK);
    CHECK(cara.roll == 3u);
    CHECK(registry_remove(&registry, 2) == SRMS_OK);
    CHECK(registry_smallest_free_roll(&registry) == 2u);
    bea.roll = 0;
    CHECK(registry_add(&registry, &bea) == SRMS_OK);
    CHECK(bea.roll == 2u);

    registry_clear(&registry);
    ann = make_student(1, "Ann", "Physics", 2020, 700);
    bea = make_student(2, "Bea", "Mathematics", 2021, 800);
    cara = make_student(3, "Cara", "Physics", 2022, 700);
    CHECK(registry_insert(&registry, &ann) == SRMS_OK);
    CHECK(registry_insert(&registry, &bea) == SRMS_OK);
    CHECK(registry_insert(&registry, &cara) == SRMS_OK);
    CHECK(registry_insert(&registry, &ann) != SRMS_OK);
    CHECK(registry_sort(&registry, SRMS_SORT_PERCENTAGE, SRMS_ORDER_ASC) == SRMS_OK);
    node = registry.head;
    CHECK(node != NULL && strcmp(node->student.name, "Ann") == 0);
    node = node->next;
    CHECK(node != NULL && strcmp(node->student.name, "Cara") == 0);
    node = node->next;
    CHECK(node != NULL && strcmp(node->student.name, "Bea") == 0);
    CHECK(node->next == NULL);
    CHECK(registry.tail == node);

    CHECK(registry_sort(&registry, SRMS_SORT_PERCENTAGE, SRMS_ORDER_DESC) == SRMS_OK);
    node = registry.head;
    CHECK(strcmp(node->student.name, "Bea") == 0);
    CHECK(strcmp(node->next->student.name, "Ann") == 0);
    CHECK(strcmp(node->next->next->student.name, "Cara") == 0);

    CHECK(registry_sort(&registry, SRMS_SORT_NAME, SRMS_ORDER_ASC) == SRMS_OK);
    CHECK(strcmp(registry.head->student.name, "Ann") == 0);
    CHECK(strcmp(registry.head->next->student.name, "Bea") == 0);

    CHECK(registry_search(&registry, "phy", hits, 4) == 2u);
    CHECK(registry_search(&registry, "0003", hits, 4) == 1u);
    CHECK(hits[0]->roll == 3u);
    CHECK(registry_search(&registry, "nope", hits, 4) == 0u);

    snprintf(cara.programme, sizeof cara.programme, "Chemistry");
    CHECK(registry_update(&registry, &cara) == SRMS_OK);
    CHECK(strcmp(registry_find_roll(&registry, 3)->programme, "Chemistry") == 0);
    CHECK(registry_find_roll(&registry, 9) == NULL);
    CHECK(registry_remove(&registry, 9) == SRMS_ERR_NOT_FOUND);

    registry_clear(&registry);
    ann = make_student(50, "Ann", "Physics", 2020, 700);
    CHECK(registry_insert(&registry, &ann) == SRMS_OK);
    CHECK(registry_smallest_free_roll(&registry) == 1u);

    registry_clear(&registry);
    for (i = 0; i < 20u; i++) {
        char name[16];
        Student student;
        snprintf(name, sizeof name, "Name %c", (char)('A' + (int)i));
        student = make_student(0, name, "Physics", 2020, 600);
        CHECK(registry_add(&registry, &student) == SRMS_OK);
    }
    CHECK(registry.count == 20u);
    registry_clear(&registry);
}

static void test_storage(void)
{
    char *dir = make_dir();
    char path[128];
    Registry registry;
    Registry loaded;
    Student ann;
    Student bea;
    FILE *fp;
    char *text = NULL;
    size_t length = 0;
    char *crc_at;
    const char *dup_payload =
        "1|Ann|Physics|2020|800|800|800|800|800\n"
        "1|Bea|Physics|2020|700|700|700|700|700\n";

    CHECK(dir != NULL);
    path_join(path, sizeof path, dir, "reg.srms");
    registry_init(&registry);
    registry_init(&loaded);
    ann = make_student(2, "Bea", "Mathematics", 2021, 810);
    bea = make_student(1, "Ann", "Physics", 2020, 900);
    CHECK(registry_insert(&registry, &bea) == SRMS_OK);
    CHECK(registry_insert(&registry, &ann) == SRMS_OK);
    CHECK(storage_save(&registry, path) == SRMS_OK);
    CHECK(storage_load(&loaded, path) == SRMS_OK);
    CHECK(loaded.count == 2u);
    CHECK(loaded.head->student.roll == 1u);
    CHECK(strcmp(loaded.head->next->student.name, "Bea") == 0);
    CHECK(student_percentage_tenths(&loaded.head->student) == 900u);

    fp = fopen(path, "rb");
    CHECK(fp != NULL);
    text = malloc(4096);
    CHECK(text != NULL);
    length = fread(text, 1, 4095, fp);
    text[length] = '\0';
    fclose(fp);
    crc_at = strstr(text, "crc32 ");
    CHECK(crc_at != NULL);
    crc_at[6] = (char)(crc_at[6] == '0' ? '1' : '0');
    fp = fopen(path, "wb");
    CHECK(fp != NULL);
    fwrite(text, 1, length, fp);
    fclose(fp);
    CHECK(storage_load(&loaded, path) == SRMS_ERR_CHECKSUM);
    CHECK(loaded.count == 2u);

    fp = fopen(path, "wb");
    fprintf(fp, "SRMS 9\ncount 0\ncrc32 00000000\n---\n");
    fclose(fp);
    CHECK(storage_load(&loaded, path) == SRMS_ERR_FORMAT);
    CHECK(loaded.count == 2u);

    fp = fopen(path, "wb");
    fprintf(fp, "SRMS 1\ncount 2\ncrc32 %08x\n---\n%s",
            (unsigned)srms_crc32(dup_payload, strlen(dup_payload)), dup_payload);
    fclose(fp);
    CHECK(storage_load(&loaded, path) != SRMS_OK);
    CHECK(loaded.count == 2u);

    fp = fopen(path, "wb");
    fprintf(fp, "SRMS 1\ncount 0\ncrc32 00000000\n---\n");
    fclose(fp);
    CHECK(storage_load(&loaded, path) == SRMS_OK);
    CHECK(loaded.count == 0u);

    registry_clear(&registry);
    registry_clear(&loaded);
    free(text);
    free(dir);
}

static void test_stats(void)
{
    Registry registry;
    ClassStats stats;
    Student a = make_student(1, "Ann", "Physics", 2020, 500);
    Student b = make_student(2, "Bea", "Physics", 2020, 700);
    Student c = make_student(3, "Cara", "Physics", 2020, 900);

    registry_init(&registry);
    stats_compute(&registry, &stats);
    CHECK(stats.count == 0u);
    CHECK(stats.highest == NULL);

    CHECK(registry_insert(&registry, &a) == SRMS_OK);
    CHECK(registry_insert(&registry, &b) == SRMS_OK);
    CHECK(registry_insert(&registry, &c) == SRMS_OK);
    stats_compute(&registry, &stats);
    CHECK(stats.count == 3u);
    CHECK(stats.average_tenths == 700u);
    CHECK(stats.median_tenths == 700u);
    CHECK(stats.highest->roll == 3u);
    CHECK(stats.lowest->roll == 1u);
    CHECK(stats.grade_count[0] == 1u);
    CHECK(stats.grade_count[2] == 1u);
    CHECK(stats.grade_count[4] == 1u);
    CHECK(stats.top_count == 3u);
    CHECK(stats.top[0]->roll == 3u);
    CHECK(stats.top[2]->roll == 1u);
    CHECK(stats.pct_bins[1] == 1u);
    CHECK(stats.pct_bins[3] == 1u);
    CHECK(stats.pct_bins[5] == 1u);

    registry_clear(&registry);
    a = make_student(1, "Ann", "Physics", 2020, 400);
    b = make_student(2, "Bea", "Physics", 2020, 800);
    CHECK(registry_insert(&registry, &a) == SRMS_OK);
    CHECK(registry_insert(&registry, &b) == SRMS_OK);
    stats_compute(&registry, &stats);
    CHECK(stats.median_tenths == 600u);
    CHECK(stats.average_tenths == 600u);
    registry_clear(&registry);
}

static void test_csv(void)
{
    char *dir = make_dir();
    char csv_path[128];
    char quoted_path[128];
    Registry registry;
    Registry loaded;
    Student ann = make_student(4, "Ann O'Neil", "Physics", 2020, 855);
    FILE *fp;
    const char *bad =
        "name,programme,year,paper_1,paper_2,paper_3,paper_4,practical\n"
        "\"Ada, Lovelace\",Physics,2022,80,80,80,80,80\n";

    CHECK(dir != NULL);
    path_join(csv_path, sizeof csv_path, dir, "out.csv");
    path_join(quoted_path, sizeof quoted_path, dir, "bad.csv");
    registry_init(&registry);
    registry_init(&loaded);
    CHECK(registry_insert(&registry, &ann) == SRMS_OK);
    CHECK(csv_export(&registry, csv_path) == SRMS_OK);
    CHECK(csv_import(&loaded, csv_path) == SRMS_OK);
    CHECK(loaded.count == 1u);
    CHECK(loaded.head->student.roll == 4u);
    CHECK(strcmp(loaded.head->student.name, "Ann O'Neil") == 0);
    CHECK(student_percentage_tenths(&loaded.head->student) == 855u);

    fp = fopen(quoted_path, "wb");
    fputs(bad, fp);
    fclose(fp);
    CHECK(csv_import(&loaded, quoted_path) != SRMS_OK);
    CHECK(loaded.count == 1u);

    registry_clear(&registry);
    registry_clear(&loaded);
    free(dir);
}

static void test_ui_and_session(void)
{
    char *dir = make_dir();
    char path[128];
    Registry registry;
    Student ann = make_student(1, "Meera Iyer", "Physics", 2022, 964);
    UiStyle style;
    char *rendered = NULL;
    size_t rendered_len = 0;
    FILE *screen;
    char script[256];
    FILE *in;
    FILE *out;
    char *session = NULL;
    size_t session_len = 0;
    AppConfig config;
    Registry loaded;

    CHECK(dir != NULL);
    path_join(path, sizeof path, dir, "reg.srms");
    registry_init(&registry);
    CHECK(registry_insert(&registry, &ann) == SRMS_OK);
    style.color = 0;
    style.unicode = 0;
    screen = open_memstream(&rendered, &rendered_len);
    CHECK(screen != NULL);
    ui_render_register(screen, &style, &registry, "Register");
    ui_render_stats(screen, &style, &registry);
    fclose(screen);
    CHECK(strstr(rendered, "Meera Iyer") != NULL);
    CHECK(strstr(rendered, "96.4") != NULL);
    CHECK(strstr(rendered, "Fig. 1") != NULL);
    CHECK(strstr(rendered, "Fig. 2") != NULL);
    CHECK(strstr(rendered, "####") != NULL || strstr(rendered, "#") != NULL);
    free(rendered);
    registry_clear(&registry);

    snprintf(script, sizeof script,
             "1\nMeera Iyer\nPhysics\n2022\n98\n95\n99\n94\n96\ny\n\n9\n\n0\n");
    in = fmemopen(script, strlen(script), "r");
    out = open_memstream(&session, &session_len);
    CHECK(in != NULL && out != NULL);
    memset(&config, 0, sizeof config);
    config.path = path;
    config.style = style;
    config.confirm_quit = 0;
    CHECK(app_run(in, out, &config) == 0);
    fclose(in);
    fclose(out);
    CHECK(strstr(session, "Meera Iyer") != NULL);
    CHECK(strstr(session, "Wrote") != NULL);
    free(session);

    registry_init(&loaded);
    CHECK(storage_load(&loaded, path) == SRMS_OK);
    CHECK(loaded.count == 1u);
    CHECK(loaded.head->student.roll == 1u);
    CHECK(student_percentage_tenths(&loaded.head->student) == 964u);
    registry_clear(&loaded);
    free(dir);
}

static void test_corrupt_inputs(void)
{
    char *dir = make_dir();
    char path[128];
    uint32_t state = 0x12345678u;
    int i;

    CHECK(dir != NULL);
    path_join(path, sizeof path, dir, "junk.srms");
    for (i = 0; i < 30; i++) {
        unsigned char blob[96];
        size_t j;
        FILE *fp;
        Registry registry;
        for (j = 0; j < sizeof blob; j++) {
            state = state * 1664525u + 1013904223u;
            blob[j] = (unsigned char)(state >> 24);
        }
        fp = fopen(path, "wb");
        CHECK(fp != NULL);
        fwrite(blob, 1, sizeof blob, fp);
        fclose(fp);
        registry_init(&registry);
        (void)storage_load(&registry, path);
        registry_clear(&registry);
    }
    free(dir);
}

static void test_sample_cohort(void)
{
    Registry registry;
    ClassStats stats;

    registry_init(&registry);
    if (storage_load(&registry, "data/sample_cohort.srms") != SRMS_OK) {
        registry_clear(&registry);
        return;
    }
    stats_compute(&registry, &stats);
    CHECK(stats.count == 24u);
    CHECK(stats.grade_count[0] == 3u);
    CHECK(stats.grade_count[1] == 7u);
    CHECK(stats.grade_count[2] == 8u);
    CHECK(stats.grade_count[3] == 4u);
    CHECK(stats.grade_count[4] == 1u);
    CHECK(stats.grade_count[5] == 1u);
    CHECK(strcmp(stats.highest->name, "Meera Iyer") == 0);
    CHECK(strcmp(stats.lowest->name, "Ruth Adler") == 0);
    registry_clear(&registry);
}

int main(void)
{
    test_crc();
    test_student_math();
    test_validate();
    test_registry();
    test_storage();
    test_stats();
    test_csv();
    test_ui_and_session();
    test_corrupt_inputs();
    test_sample_cohort();
    printf("%d checks passed\n", srms_check_passes());
    if (srms_check_failures() != 0) {
        fprintf(stderr, "%d checks failed\n", srms_check_failures());
        return 1;
    }
    return 0;
}
