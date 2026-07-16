#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_tests.h"

#if cr_macos || cr_ios

struct tests_refs_dir_info {
    char name[80];
    uint32_t item_count;
    uint64_t size_bytes;
};

const char* tests_refs_name_(void);
uint32_t tests_refs_dir_list_(struct tests_refs_dir_info* out, uint32_t capacity);
bool tests_refs_dir_delete_(const char* name);
bool tests_refs_has_(struct ui_test_scenes* scenes, uint32_t index);
bool tests_refs_load_graphic_(struct ui_test_scenes* scenes, uint32_t index, struct ui_graphic** out);

#endif

#if cr_macos || cr_ios || cr_wasm

float tests_refs_use_(const char* ref);
void tests_refs_test_(struct ui_test_scenes* scenes, uint32_t iterations, bool* out_pass);

#endif

#if cr_ios

void tests_refs_capture_(struct ui_test_scenes* scenes, double screen_to_px_from_pt, double2 screen_size, const char* only);

#endif
