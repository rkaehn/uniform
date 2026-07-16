#pragma once

#include "cr.h"

enum cr_closed app_event_type {
    app_event_type_pointer,
    app_event_type_scroll,
    app_event_type_key,
    app_event_type_text,
    app_event_type_marked_text,
    app_event_type_text_command,
    app_event_type_floating_caret,
    app_event_type_text_input_dismissed,
    app_event_type_cancel_all,
};

enum cr_closed app_text_command {
    app_text_command_move_left,
    app_text_command_move_right,
    app_text_command_move_up,
    app_text_command_move_down,
    app_text_command_move_word_left,
    app_text_command_move_word_right,
    app_text_command_move_line_start,
    app_text_command_move_line_end,
    app_text_command_move_doc_start,
    app_text_command_move_doc_end,
    app_text_command_delete_backward,
    app_text_command_delete_forward,
    app_text_command_delete_word_backward,
    app_text_command_delete_word_forward,
    app_text_command_insert_newline,
    app_text_command_insert_tab,
    app_text_command_select_all,
    app_text_command_copy,
    app_text_command_cut,
    app_text_command_delete_line,
    app_text_command_delete_line_start,
    app_text_command_delete_line_end,
    app_text_command_move_page_up,
    app_text_command_move_page_down,
    app_text_command_scroll_doc_start,
    app_text_command_scroll_doc_end,
    app_text_command_scroll_page_up,
    app_text_command_scroll_page_down,
};

enum cr_closed app_pointer_event_type {
    app_pointer_event_type_hover_began,
    app_pointer_event_type_hover_moved,
    app_pointer_event_type_hover_ended,
    app_pointer_event_type_touch_began,
    app_pointer_event_type_touch_moved,
    app_pointer_event_type_touch_ended,
    app_pointer_event_type_canceled,
};

enum cr_closed app_pointer_source {
    app_pointer_source_direct,
    app_pointer_source_indirect,
    app_pointer_source_pencil,
};

enum cr_closed app_pointer_button {
    app_pointer_button_primary,
    app_pointer_button_secondary,
    app_pointer_button_middle,
};

enum cr_closed_flag app_modifier {
    app_modifier_shift = 1 << 0,
    app_modifier_control = 1 << 1,
    app_modifier_alt = 1 << 2,
    app_modifier_command = 1 << 3,
};

enum cr_closed app_key {
    app_key_none = 0x00,
    app_key_a = 0x04,
    app_key_b = 0x05,
    app_key_c = 0x06,
    app_key_d = 0x07,
    app_key_e = 0x08,
    app_key_f = 0x09,
    app_key_g = 0x0a,
    app_key_h = 0x0b,
    app_key_i = 0x0c,
    app_key_j = 0x0d,
    app_key_k = 0x0e,
    app_key_l = 0x0f,
    app_key_m = 0x10,
    app_key_n = 0x11,
    app_key_o = 0x12,
    app_key_p = 0x13,
    app_key_q = 0x14,
    app_key_r = 0x15,
    app_key_s = 0x16,
    app_key_t = 0x17,
    app_key_u = 0x18,
    app_key_v = 0x19,
    app_key_w = 0x1a,
    app_key_x = 0x1b,
    app_key_y = 0x1c,
    app_key_z = 0x1d,
    app_key_1 = 0x1e,
    app_key_2 = 0x1f,
    app_key_3 = 0x20,
    app_key_4 = 0x21,
    app_key_5 = 0x22,
    app_key_6 = 0x23,
    app_key_7 = 0x24,
    app_key_8 = 0x25,
    app_key_9 = 0x26,
    app_key_0 = 0x27,
    app_key_return = 0x28,
    app_key_escape = 0x29,
    app_key_backspace = 0x2a,
    app_key_tab = 0x2b,
    app_key_space = 0x2c,
    app_key_minus = 0x2d,
    app_key_equal = 0x2e,
    app_key_bracket_left = 0x2f,
    app_key_bracket_right = 0x30,
    app_key_backslash = 0x31,
    app_key_semicolon = 0x33,
    app_key_quote = 0x34,
    app_key_grave = 0x35,
    app_key_comma = 0x36,
    app_key_period = 0x37,
    app_key_slash = 0x38,
    app_key_caps_lock = 0x39,
    app_key_f1 = 0x3a,
    app_key_f2 = 0x3b,
    app_key_f3 = 0x3c,
    app_key_f4 = 0x3d,
    app_key_f5 = 0x3e,
    app_key_f6 = 0x3f,
    app_key_f7 = 0x40,
    app_key_f8 = 0x41,
    app_key_f9 = 0x42,
    app_key_f10 = 0x43,
    app_key_f11 = 0x44,
    app_key_f12 = 0x45,
    app_key_home = 0x4a,
    app_key_page_up = 0x4b,
    app_key_delete = 0x4c,
    app_key_end = 0x4d,
    app_key_page_down = 0x4e,
    app_key_right = 0x4f,
    app_key_left = 0x50,
    app_key_down = 0x51,
    app_key_up = 0x52,
    app_key_keypad_divide = 0x54,
    app_key_keypad_multiply = 0x55,
    app_key_keypad_minus = 0x56,
    app_key_keypad_plus = 0x57,
    app_key_keypad_enter = 0x58,
    app_key_keypad_1 = 0x59,
    app_key_keypad_2 = 0x5a,
    app_key_keypad_3 = 0x5b,
    app_key_keypad_4 = 0x5c,
    app_key_keypad_5 = 0x5d,
    app_key_keypad_6 = 0x5e,
    app_key_keypad_7 = 0x5f,
    app_key_keypad_8 = 0x60,
    app_key_keypad_9 = 0x61,
    app_key_keypad_0 = 0x62,
    app_key_keypad_period = 0x63,
    app_key_control_left = 0xe0,
    app_key_shift_left = 0xe1,
    app_key_alt_left = 0xe2,
    app_key_command_left = 0xe3,
    app_key_control_right = 0xe4,
    app_key_shift_right = 0xe5,
    app_key_alt_right = 0xe6,
    app_key_command_right = 0xe7,
};

enum cr_closed app_cursor {
    app_cursor_default,
    app_cursor_text,
};

enum cr_closed app_floating_caret_phase {
    app_floating_caret_phase_began,
    app_floating_caret_phase_moved,
    app_floating_caret_phase_ended,
};

struct app_pointer_event {
    enum app_pointer_event_type type;
    enum app_pointer_source source;
    enum app_pointer_button button;
    uint64_t id;
    uint32_t modifiers;
    double pressure;
    double radius;
    double2 pos;
    double timestamp;
};

struct app_scroll_event {
    double2 delta;
    double2 pos;
    double timestamp;
};

struct app_key_event {
    enum app_key key;
    uint32_t modifiers;
    bool down;
    bool repeat;
};

struct app_text_event {
    const char* utf8;
    uint32_t size;
};

struct app_marked_text_event {
    const char* utf8;
    uint32_t size;
    uint32_t caret;
};

struct app_text_command_event {
    enum app_text_command command;
    bool select;
};

struct app_floating_caret_event {
    enum app_floating_caret_phase phase;
    double2 pos;
};

struct app_event {
    enum app_event_type type;
    union {
        struct app_pointer_event pointer;
        struct app_scroll_event scroll;
        struct app_key_event key;
        struct app_text_event text;
        struct app_marked_text_event marked_text;
        struct app_text_command_event text_command;
        struct app_floating_caret_event floating_caret;
    };
};
