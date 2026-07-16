#pragma once

#include "cr.h"

enum cr_closed log_mode {
    log_mode_tool,
    log_mode_term,
    log_mode_both,
};

enum cr_closed log_pref {
    log_pref_mode,
    log_pref_freeze_on_next_open,
    log_pref_is_visible,
    log_pref_width,
    log_pref_height,

    log_pref_count,
};

int log_init(const char* path);
void log_deinit(void);

bool log_is_active(void);

void log_dump(void);
void log_clear(void);
void log_pause(void);
void log_resume(void);
bool log_is_paused(void);
void log_set_mode(enum log_mode mode);

uint32_t log_view_row_count(uint32_t view_width_chars);
void log_view_glyphs(uint32_t view_width_chars, int32_t row_start, int32_t row_end, void (*func)(int32_t row, uint32_t col, uint32_t cp, void* user), void* user);

void log_set_pref(uint32_t slot, uint16_t value);
uint16_t log_get_pref(uint32_t slot);

__attribute__((format(printf, 1, 2)))
void log_s(const char* fmt, ...);
void log_str(const char* str, uint64_t size);
