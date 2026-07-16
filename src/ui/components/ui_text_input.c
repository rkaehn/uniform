#include "cr.h"
#include "ui.h"
#include "ui_input.h"
#include "ui_gestures.h"
#include "ui_text_input.h"
#include "ui_physics.h"
#include <string.h>
#include <math.h>

#define ui_text_input_blink_period_s 1.1
#define ui_text_input_blink_on_s 0.6
#define ui_text_input_multi_click_interval_s 0.45
#define ui_text_input_multi_click_slop_pt 6.0
#define ui_text_input_touch_slop_pt 15.0
#define ui_text_input_long_press_s 0.45
#define ui_text_input_handle_grab_pt 24.0
#define ui_text_input_handle_knob_offset_pt 6.0
#define ui_text_input_caret_slack_pt 2.0
#define ui_text_input_autoscroll_per_s 8.0

enum ui_text_input_handle {
    ui_text_input_handle_none,
    ui_text_input_handle_start,
    ui_text_input_handle_end,
};

struct ui_text_input;
static uint32_t ui_text_input_handle_hit(struct ui_text_input* input, double2 root_pos);
static double2 ui_text_input_text_pos(struct ui_text_input* input, double2 root_pos);

struct ui_text_input {
    struct cr_mem* mem;
    char* buffer;
    uint64_t size;
    uint64_t capacity;
    uint32_t caret;
    uint32_t anchor;
    uint32_t marked_start;
    uint32_t marked_size;
    bool has_marked;
    bool caret_upstream;
    bool is_dragging;
    bool drag_caret_only;
    bool touch_selection;
    bool scroll_to_caret;
    bool has_layout;
    bool preferred_x_valid;
    uint32_t pan_handle;
    double pan_last_x;
    double preferred_x;
    double blink_s;
    double last_press_timestamp;
    double2 last_press_pos;
    double2 last_drag_pos;
    uint32_t click_count;
    uint32_t drag_origin_start;
    uint32_t drag_origin_end;
    struct ui_scroll scroll;
    double scroll_x;
    double last_view_height;
    double last_text_height;
    uint64_t focus_key;
    bool focus_pending;
    double2 applied_content_pos;
    struct ui_node prev_root;
    struct ui_node prev_main;
    struct ui_text_input_layout last_layout;
    struct ui_node scratch;
    bool scratch_valid;
    struct ui_text_edit_rect* selection_rects;
    uint32_t selection_rect_capacity;
    struct ui_text_edit_rect* marked_rects;
    uint32_t marked_rect_capacity;
};

static void ui_text_input_reserve_rects(struct ui_text_input* input, struct ui_text_edit_rect** rects, uint32_t* capacity, uint32_t needed) {
    if (*capacity >= needed) {
        return;
    }
    if (*rects) {
        cr_free(input->mem, *rects, cr_loc);
    }
    *capacity = max_uint32(needed, 16);
    *rects = cr_malloc(input->mem, *capacity * sizeof(**rects), 0, cr_loc);
}

struct ui_text_input* ui_text_input_create(struct cr_mem* mem) {
    struct ui_text_input* input = cr_malloc(mem, sizeof(*input), 0, cr_loc);

    memset(input, 0, sizeof(*input));
    input->mem = mem;
    input->capacity = 64;
    input->buffer = cr_malloc(mem, input->capacity, 0, cr_loc);
    input->buffer[0] = 0;
    input->last_press_timestamp = -1.0e9;
    ui_scroll_init(&input->scroll);

    return input;
}

void ui_text_input_destroy(struct ui_text_input* input) {
    if (ui_node_is_null(input->scratch) == false) {
        ui_release(input->scratch);
    }
    if (input->selection_rects) {
        cr_free(input->mem, input->selection_rects, cr_loc);
    }
    if (input->marked_rects) {
        cr_free(input->mem, input->marked_rects, cr_loc);
    }
    cr_free(input->mem, input->buffer, cr_loc);
    cr_free(input->mem, input, cr_loc);
}

static void ui_text_input_replace(struct ui_text_input* input, uint32_t start, uint32_t end, const char* utf8, uint32_t insert_size) {
    assert(start <= end && end <= input->size);

    uint64_t new_size = input->size - (end - start) + insert_size;

    if (new_size + 1 > input->capacity) {
        uint64_t new_capacity = input->capacity;
        while (new_capacity < new_size + 1) {
            new_capacity *= 2;
        }
        char* new_buffer = cr_malloc(input->mem, new_capacity, 0, cr_loc);
        memcpy(new_buffer, input->buffer, (unsigned long)(input->size + 1));
        cr_free(input->mem, input->buffer, cr_loc);
        input->buffer = new_buffer;
        input->capacity = new_capacity;
    }

    memmove(input->buffer + start + insert_size, input->buffer + end, (unsigned long)(input->size - end));
    if (insert_size > 0) {
        memcpy(input->buffer + start, utf8, insert_size);
    }
    input->size = new_size;
    input->buffer[new_size] = 0;
    input->scratch_valid = false;
}

static void ui_text_input_changed(struct ui_text_input* input) {
    input->blink_s = 0.0;
    input->caret_upstream = false;
    input->preferred_x_valid = false;
    input->scroll_to_caret = true;
    input->last_press_timestamp = -1.0e9;

}

void ui_text_input_set_text(struct ui_text_input* input, const char* utf8, uint64_t size) {
    ui_text_input_replace(input, 0, (uint32_t)input->size, utf8, (uint32_t)size);
    input->caret = (uint32_t)input->size;
    input->anchor = input->caret;
    input->has_marked = false;
    input->marked_size = 0;
    input->preferred_x_valid = false;
    input->scroll_to_caret = true;
}

void ui_text_input_get_text(struct ui_text_input* input, const char** out_utf8, uint64_t* out_size) {
    *out_utf8 = input->buffer;
    *out_size = input->size;
}

bool ui_text_input_is_focused(struct ui_text_input* input) {
    return input->focus_key != 0 && ui_input_get_focus() == input->focus_key;
}

void ui_text_input_set_focus(struct ui_text_input* input, bool focused) {
    if (focused) {
        if (input->focus_key != 0) {
            ui_input_set_focus(input->focus_key);
        } else {
            input->focus_pending = true;
        }
        input->blink_s = 0.0;
        input->scroll_to_caret = true;
    } else {
        input->focus_pending = false;
        if (ui_text_input_is_focused(input)) {
            ui_input_set_focus(0);
        }
    }
}

static struct ui_str ui_text_input_make_str(struct ui_text_input* input, const struct ui_text_input_layout* layout) {
    return (struct ui_str) {
        .raw = input->buffer,
        .attributes = {
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = layout->pt_size },
            { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_weight, .weight = layout->weight },
            layout->font
                ? (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = layout->font }
                : (struct ui_str_attribute) { 0 },
        },
    };
}

static double ui_text_input_text_width(const struct ui_text_input_layout* layout) {
    return max_double(layout->width - (double)layout->padding.x - (double)layout->padding.y, 1.0);
}

static struct ui_node ui_text_input_text_node(struct ui_text_input* input) {
    if (input->scratch_valid) {
        return input->scratch;
    }

    if (ui_node_is_null(input->scratch) == false) {
        ui_release(input->scratch);
    }

    struct ui_str str = ui_text_input_make_str(input, &input->last_layout);

    struct ui_text desc = {
        .str = str,
    };
    if (input->last_layout.multiline) {
        desc.size.width = ui_size_axis_fix(ui_text_input_text_width(&input->last_layout));
    }

    input->scratch = ui_text(&desc);
    ui_layout(input->scratch);
    input->scratch_valid = true;

    return input->scratch;
}

static uint32_t ui_text_input_line_index(struct ui_text_input* input, uint32_t offset) {
    struct ui_node text = ui_text_input_text_node(input);
    for (uint32_t i = ui_text_get_line_count(text); i-- > 1;) {
        if (offset >= ui_text_get_line(text, i).start) {
            return i;
        }
    }
    return 0;
}

static bool ui_text_input_line_ends_soft(struct ui_text_input* input, uint32_t line_index, uint32_t offset) {
    struct ui_node text = ui_text_input_text_node(input);
    struct ui_text_line line = ui_text_get_line(text, line_index);
    return line_index + 1 < ui_text_get_line_count(text) && line.end == offset && line.visual_end == offset;
}

static uint32_t ui_text_input_line_index_affine(struct ui_text_input* input, uint32_t offset, bool upstream) {
    uint32_t index = ui_text_input_line_index(input, offset);
    if (upstream && index > 0 && ui_text_input_line_ends_soft(input, index - 1, offset)) {
        return index - 1;
    }
    return index;
}

static bool ui_text_input_is_soft_boundary(struct ui_text_input* input, uint32_t offset) {
    uint32_t index = ui_text_input_line_index(input, offset);
    return index > 0 && ui_text_input_line_ends_soft(input, index - 1, offset);
}

static struct ui_text_edit_rect ui_text_input_caret_rect(struct ui_text_input* input, uint32_t offset, bool upstream) {
    struct ui_node text = ui_text_input_text_node(input);
    uint32_t index = ui_text_input_line_index_affine(input, offset, upstream);
    struct ui_text_line line = ui_text_get_line(text, index);

    return (struct ui_text_edit_rect) {
        .x = ui_text_get_x(text, index, offset),
        .y = line.y,
        .width = 0.0,
        .height = line.height,
    };
}

static uint32_t ui_text_input_offset_at(struct ui_text_input* input, double2 pos, bool* out_upstream) {
    struct ui_node text = ui_text_input_text_node(input);
    uint32_t line_count = ui_text_get_line_count(text);

    uint32_t line_index = 0;
    while (line_index + 1 < line_count) {
        struct ui_text_line line = ui_text_get_line(text, line_index);
        if (pos.y < line.y + line.height) {
            break;
        }
        line_index++;
    }

    uint32_t offset = ui_text_get_offset(text, line_index, pos.x);
    if (out_upstream) {
        *out_upstream = ui_text_input_line_ends_soft(input, line_index, offset);
    }
    return offset;
}

static uint32_t ui_text_input_offset_for_line_move(struct ui_text_input* input, uint32_t offset, bool upstream, int32_t line_delta, double preferred_x, bool* out_upstream) {
    struct ui_node text = ui_text_input_text_node(input);

    if (out_upstream) {
        *out_upstream = false;
    }

    int64_t target = (int64_t)ui_text_input_line_index_affine(input, offset, upstream) + line_delta;

    if (target < 0) {
        return 0;
    }
    if (target >= ui_text_get_line_count(text)) {
        return (uint32_t)input->size;
    }

    uint32_t result = ui_text_get_offset(text, (uint32_t)target, preferred_x);
    if (out_upstream) {
        *out_upstream = ui_text_input_line_ends_soft(input, (uint32_t)target, result);
    }
    return result;
}

static void ui_text_input_line_range(struct ui_text_input* input, uint32_t offset, bool upstream, uint32_t* out_start, uint32_t* out_end) {
    struct ui_node text = ui_text_input_text_node(input);
    struct ui_text_line line = ui_text_get_line(text, ui_text_input_line_index_affine(input, offset, upstream));
    *out_start = line.start;
    *out_end = line.visual_end;
}

static uint32_t ui_text_input_range_rects(struct ui_text_input* input, uint32_t start, uint32_t end, struct ui_text_edit_rect* out_rects, uint32_t out_rect_capacity) {
    struct ui_node text = ui_text_input_text_node(input);
    uint32_t line_count = ui_text_get_line_count(text);
    uint32_t count = 0;

    for (uint32_t i = 0; i < line_count; i++) {
        struct ui_text_line line = ui_text_get_line(text, i);

        if (end <= line.start || start >= line.end) {
            continue;
        }

        uint32_t seg_start = max_uint32(start, line.start);
        uint32_t seg_end = min_uint32(end, line.visual_end);

        double x0 = ui_text_get_x(text, i, seg_start);
        double x1 = seg_end > seg_start ? ui_text_get_x(text, i, seg_end) : x0;

        if (end > line.visual_end && i + 1 < line_count) {
            x1 += line.height * 0.25;
        }

        double y0 = ui_round_px(line.y);
        double y1 = ui_round_px(line.y + line.height);

        if (x1 > x0 && count < out_rect_capacity) {
            out_rects[count++] = (struct ui_text_edit_rect) {
                .x = x0,
                .y = y0,
                .width = x1 - x0,
                .height = y1 - y0,
            };
        }
    }

    return count;
}

static double ui_text_input_lines_height(struct ui_text_input* input) {
    struct ui_node text = ui_text_input_text_node(input);
    struct ui_text_line last = ui_text_get_line(text, ui_text_get_line_count(text) - 1);
    return last.y + last.height;
}

static double ui_text_input_lines_width(struct ui_text_input* input) {
    struct ui_node text = ui_text_input_text_node(input);
    uint32_t line_count = ui_text_get_line_count(text);
    double width = 0.0;
    for (uint32_t i = 0; i < line_count; i++) {
        width = max_double(width, ui_text_get_line(text, i).width);
    }
    return width;
}

static uint32_t ui_text_input_sel_start(struct ui_text_input* input) {
    return min_uint32(input->caret, input->anchor);
}

static uint32_t ui_text_input_sel_end(struct ui_text_input* input) {
    return max_uint32(input->caret, input->anchor);
}

enum ui_text_input_char_kind {
    ui_text_input_char_kind_space,
    ui_text_input_char_kind_newline,
    ui_text_input_char_kind_punct,
    ui_text_input_char_kind_word,
    ui_text_input_char_kind_ideograph,
    ui_text_input_char_kind_hiragana,
    ui_text_input_char_kind_katakana,
    ui_text_input_char_kind_hangul,
};

static uint32_t ui_text_input_decode_at(const char* raw, uint32_t size, uint32_t offset, uint32_t* out_next) {
    uint8_t lead = (uint8_t)raw[offset];
    if (lead < 0x80) {
        *out_next = offset + 1;
        return lead;
    }

    uint32_t length = lead >= 0xf0 ? 4 : (lead >= 0xe0 ? 3 : (lead >= 0xc0 ? 2 : 0));

    if (length == 0 || offset + length > size) {
        *out_next = offset + 1;
        return 0xfffd;
    }

    uint32_t codepoint = lead & (uint32_t)(0xff >> (length + 1));
    for (uint32_t i = 1; i < length; i++) {
        uint8_t byte = (uint8_t)raw[offset + i];
        if ((byte & 0xc0) != 0x80) {
            *out_next = offset + 1;
            return 0xfffd;
        }
        codepoint = (codepoint << 6) | (byte & 0x3f);
    }

    *out_next = offset + length;
    return codepoint;
}

static uint32_t ui_text_input_decode_before(const char* raw, uint32_t size, uint32_t offset, uint32_t* out_prev) {
    uint32_t start = offset - 1;
    while (start > 0 && ((uint8_t)raw[start] & 0xc0) == 0x80 && offset - start < 4) {
        start--;
    }

    uint32_t next;
    uint32_t codepoint = ui_text_input_decode_at(raw, size, start, &next);
    if (next != offset) {
        *out_prev = offset - 1;
        return 0xfffd;
    }

    *out_prev = start;
    return codepoint;
}

static uint32_t ui_text_input_char_kind(uint32_t cp) {
    if (cp == ' ' || cp == '\t' || cp == 0x00a0 || cp == 0x3000 || (cp >= 0x2000 && cp <= 0x200a)) {
        return ui_text_input_char_kind_space;
    }
    if (cp == '\n' || cp == '\r') {
        return ui_text_input_char_kind_newline;
    }
    if (cp < 0x80) {
        bool word = (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == '_';
        return word ? ui_text_input_char_kind_word : ui_text_input_char_kind_punct;
    }
    if ((cp >= 0x2010 && cp <= 0x2027) || (cp >= 0x2030 && cp <= 0x205e)) {
        return ui_text_input_char_kind_punct;
    }
    if ((cp >= 0x3001 && cp <= 0x3020) || (cp >= 0xfe30 && cp <= 0xfe4f) || cp == 0x30fb) {
        return ui_text_input_char_kind_punct;
    }
    if ((cp >= 0xff01 && cp <= 0xff0f) || (cp >= 0xff1a && cp <= 0xff20) || (cp >= 0xff3b && cp <= 0xff40) || (cp >= 0xff5b && cp <= 0xff65)) {
        return ui_text_input_char_kind_punct;
    }
    if (cp >= 0x3041 && cp <= 0x309f) {
        return ui_text_input_char_kind_hiragana;
    }
    if ((cp >= 0x30a0 && cp <= 0x30ff) || (cp >= 0x31f0 && cp <= 0x31ff)) {
        return ui_text_input_char_kind_katakana;
    }
    if ((cp >= 0x3400 && cp <= 0x4dbf) || (cp >= 0x4e00 && cp <= 0x9fff) || (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0x20000 && cp <= 0x2fa1f)) {
        return ui_text_input_char_kind_ideograph;
    }
    if ((cp >= 0xac00 && cp <= 0xd7af) || (cp >= 0x1100 && cp <= 0x11ff)) {
        return ui_text_input_char_kind_hangul;
    }
    return ui_text_input_char_kind_word;
}

static bool ui_text_input_kind_is_word(uint32_t kind) {
    return kind >= ui_text_input_char_kind_word;
}

static uint32_t ui_text_input_word_left(struct ui_text_input* input, uint32_t offset) {
    uint32_t run_kind = uint32_max;

    while (offset > 0) {
        uint32_t prev;
        uint32_t kind = ui_text_input_char_kind(ui_text_input_decode_before(input->buffer, (uint32_t)input->size, offset, &prev));

        if (run_kind == uint32_max) {
            if (ui_text_input_kind_is_word(kind) == false) {
                offset = prev;
                continue;
            }
            run_kind = kind;
        }
        if (kind != run_kind) {
            break;
        }
        offset = prev;
    }

    return offset;
}

static uint32_t ui_text_input_word_right(struct ui_text_input* input, uint32_t offset) {
    uint32_t run_kind = uint32_max;

    while (offset < input->size) {
        uint32_t next;
        uint32_t kind = ui_text_input_char_kind(ui_text_input_decode_at(input->buffer, (uint32_t)input->size, offset, &next));

        if (run_kind == uint32_max) {
            if (ui_text_input_kind_is_word(kind) == false) {
                offset = next;
                continue;
            }
            run_kind = kind;
        }
        if (kind != run_kind) {
            break;
        }
        offset = next;
    }

    return offset;
}

static void ui_text_input_word_range(struct ui_text_input* input, uint32_t offset, uint32_t* out_start, uint32_t* out_end) {
    uint32_t size = (uint32_t)input->size;
    uint32_t next;
    uint32_t prev;
    uint32_t kind_after = offset < size ? ui_text_input_char_kind(ui_text_input_decode_at(input->buffer, size, offset, &next)) : uint32_max;
    uint32_t kind_before = offset > 0 ? ui_text_input_char_kind(ui_text_input_decode_before(input->buffer, size, offset, &prev)) : uint32_max;

    uint32_t kind = uint32_max;
    if (kind_after != uint32_max && ui_text_input_kind_is_word(kind_after)) {
        kind = kind_after;
    } else if (kind_before != uint32_max && ui_text_input_kind_is_word(kind_before)) {
        kind = kind_before;
    } else if (kind_after != uint32_max && kind_after != ui_text_input_char_kind_newline) {
        kind = kind_after;
    } else if (kind_before != uint32_max && kind_before != ui_text_input_char_kind_newline) {
        kind = kind_before;
    }

    uint32_t start = offset;
    uint32_t end = offset;

    if (kind == uint32_max) {
        if (offset < size) {
            ui_text_input_decode_at(input->buffer, size, offset, &end);
        }
    } else {
        while (start > 0 && ui_text_input_char_kind(ui_text_input_decode_before(input->buffer, size, start, &prev)) == kind) {
            start = prev;
        }
        while (end < size && ui_text_input_char_kind(ui_text_input_decode_at(input->buffer, size, end, &next)) == kind) {
            end = next;
        }
    }

    *out_start = start;
    *out_end = end;
}

static void ui_text_input_paragraph_range(struct ui_text_input* input, uint32_t offset, uint32_t* out_start, uint32_t* out_end) {
    uint32_t start = offset;
    uint32_t end = offset;

    while (start > 0 && input->buffer[start - 1] != '\n') {
        start--;
    }
    while (end < input->size && input->buffer[end] != '\n') {
        end++;
    }
    if (end < input->size) {
        end++;
    }

    *out_start = start;
    *out_end = end;
}

static void ui_text_input_move_caret(struct ui_text_input* input, uint32_t target, bool select, bool keep_preferred_x) {
    input->caret = target;
    input->caret_upstream = false;
    if (select == false) {
        input->anchor = target;
    }
    input->blink_s = 0.0;
    input->scroll_to_caret = true;
    if (keep_preferred_x == false) {
        input->preferred_x_valid = false;
    }
}

static void ui_text_input_insert(struct ui_text_input* input, const char* utf8, uint32_t size) {
    char stack[512];
    char* filtered = NULL;

    if (input->last_layout.multiline == false) {
        filtered = size <= sizeof(stack) ? stack : cr_malloc(input->mem, size, 0, cr_loc);
        for (uint32_t i = 0; i < size; i++) {
            filtered[i] = utf8[i] == '\n' || utf8[i] == '\r' ? ' ' : utf8[i];
        }
        utf8 = filtered;
    }

    uint32_t start;
    uint32_t end;

    if (input->has_marked) {
        start = input->marked_start;
        end = input->marked_start + input->marked_size;
        input->has_marked = false;
        input->marked_size = 0;
    } else {
        start = ui_text_input_sel_start(input);
        end = ui_text_input_sel_end(input);
    }

    ui_text_input_replace(input, start, end, utf8, size);
    input->caret = start + size;
    input->anchor = input->caret;
    ui_text_input_changed(input);

    if (filtered != NULL && filtered != stack) {
        cr_free(input->mem, filtered, cr_loc);
    }
}

static void ui_text_input_handle_marked(struct ui_text_input* input, const struct app_marked_text_event* event) {
    uint32_t start;
    uint32_t end;

    if (input->has_marked) {
        start = input->marked_start;
        end = input->marked_start + input->marked_size;
    } else {
        start = ui_text_input_sel_start(input);
        end = ui_text_input_sel_end(input);
    }

    ui_text_input_replace(input, start, end, event->utf8, event->size);

    if (event->size > 0) {
        input->has_marked = true;
        input->marked_start = start;
        input->marked_size = event->size;
        input->caret = start + min_uint32(event->caret, event->size);
    } else {
        input->has_marked = false;
        input->marked_size = 0;
        input->caret = start;
    }
    input->anchor = input->caret;
    ui_text_input_changed(input);
}

static void ui_text_input_delete_range(struct ui_text_input* input, uint32_t start, uint32_t end) {
    if (start == end) {
        return;
    }
    ui_text_input_replace(input, start, end, NULL, 0);
    input->caret = start;
    input->anchor = start;
    ui_text_input_changed(input);
}

static void ui_text_input_handle_command(struct ui_text_input* input, enum app_text_command command, bool select) {
    input->has_marked = false;
    input->marked_size = 0;

    uint32_t sel_start = ui_text_input_sel_start(input);
    uint32_t sel_end = ui_text_input_sel_end(input);
    bool has_selection = sel_start != sel_end;

    switch (command) {
    case app_text_command_move_left: {
        if (has_selection && select == false) {
            ui_text_input_move_caret(input, sel_start, false, false);
        } else {
            ui_text_input_move_caret(input, ui_text_prev_cluster(ui_text_input_text_node(input), input->caret), select, false);
        }
    } break;
    case app_text_command_move_right: {
        if (has_selection && select == false) {
            ui_text_input_move_caret(input, sel_end, false, false);
        } else {
            ui_text_input_move_caret(input, ui_text_next_cluster(ui_text_input_text_node(input), input->caret), select, false);
        }
    } break;
    case app_text_command_move_up:
    case app_text_command_move_down: {
        if (has_selection && select == false) {
            input->caret = command == app_text_command_move_up ? sel_start : sel_end;
            input->caret_upstream = false;
            input->preferred_x_valid = false;
        }
        if (input->preferred_x_valid == false) {
            input->preferred_x = ui_text_input_caret_rect(input, input->caret, input->caret_upstream).x;
            input->preferred_x_valid = true;
        }
        int32_t delta = command == app_text_command_move_up ? -1 : 1;
        bool upstream = false;
        uint32_t target = ui_text_input_offset_for_line_move(input, input->caret, input->caret_upstream, delta, input->preferred_x, &upstream);
        ui_text_input_move_caret(input, target, select, true);
        input->caret_upstream = upstream;
    } break;
    case app_text_command_move_word_left: {
        ui_text_input_move_caret(input, ui_text_input_word_left(input, input->caret), select, false);
    } break;
    case app_text_command_move_word_right: {
        ui_text_input_move_caret(input, ui_text_input_word_right(input, input->caret), select, false);
    } break;
    case app_text_command_move_line_start:
    case app_text_command_move_line_end: {
        uint32_t line_start;
        uint32_t line_end;
        ui_text_input_line_range(input, input->caret, input->caret_upstream, &line_start, &line_end);
        ui_text_input_move_caret(input, command == app_text_command_move_line_start ? line_start : line_end, select, false);
        if (command == app_text_command_move_line_end) {
            input->caret_upstream = ui_text_input_is_soft_boundary(input, line_end);
        }
    } break;
    case app_text_command_move_doc_start: {
        ui_text_input_move_caret(input, 0, select, false);
    } break;
    case app_text_command_move_doc_end: {
        ui_text_input_move_caret(input, (uint32_t)input->size, select, false);
    } break;
    case app_text_command_move_page_up:
    case app_text_command_move_page_down: {
        if (has_selection && select == false) {
            input->caret = command == app_text_command_move_page_up ? sel_start : sel_end;
            input->caret_upstream = false;
            input->preferred_x_valid = false;
        }
        struct ui_text_edit_rect caret_rect = ui_text_input_caret_rect(input, input->caret, input->caret_upstream);
        if (input->preferred_x_valid == false) {
            input->preferred_x = caret_rect.x;
            input->preferred_x_valid = true;
        }
        double page = max_double(input->last_view_height, caret_rect.height);
        double target_y = caret_rect.y + caret_rect.height * 0.5 + (command == app_text_command_move_page_up ? -page : page);
        bool upstream = false;
        uint32_t target = ui_text_input_offset_at(input, make_double2(input->preferred_x, target_y), &upstream);
        ui_text_input_move_caret(input, target, select, true);
        input->caret_upstream = upstream;
    } break;
    case app_text_command_scroll_doc_start:
    case app_text_command_scroll_doc_end: {
        if (input->last_layout.multiline) {
            double max_offset = max_double(input->last_text_height - input->last_view_height, 0.0);
            input->scroll.offset = command == app_text_command_scroll_doc_start ? 0.0 : max_offset;
            input->scroll.velocity = 0.0;
        } else {
            input->scroll_x = command == app_text_command_scroll_doc_start ? 0.0 : 1.0e9;
        }
    } break;
    case app_text_command_scroll_page_up:
    case app_text_command_scroll_page_down: {
        if (input->last_layout.multiline) {
            double max_offset = max_double(input->last_text_height - input->last_view_height, 0.0);
            double delta = command == app_text_command_scroll_page_up ? -input->last_view_height : input->last_view_height;
            input->scroll.offset = clamp_double(input->scroll.offset + delta, 0.0, max_offset);
            input->scroll.velocity = 0.0;
        }
    } break;
    case app_text_command_delete_backward: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else if (input->caret > 0) {
            ui_text_input_delete_range(input, ui_text_prev_cluster(ui_text_input_text_node(input), input->caret), input->caret);
        }
    } break;
    case app_text_command_delete_forward: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else if (input->caret < input->size) {
            ui_text_input_delete_range(input, input->caret, ui_text_next_cluster(ui_text_input_text_node(input), input->caret));
        }
    } break;
    case app_text_command_delete_word_backward: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else {
            ui_text_input_delete_range(input, ui_text_input_word_left(input, input->caret), input->caret);
        }
    } break;
    case app_text_command_delete_word_forward: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else {
            ui_text_input_delete_range(input, input->caret, ui_text_input_word_right(input, input->caret));
        }
    } break;
    case app_text_command_insert_newline: {
        if (input->last_layout.multiline) {
            ui_text_input_insert(input, "\n", 1);
        } else {
            ui_text_input_set_focus(input, false);
        }
    } break;
    case app_text_command_insert_tab: {
        ui_text_input_insert(input, "    ", 4);
    } break;
    case app_text_command_select_all: {
        input->anchor = 0;
        input->caret = (uint32_t)input->size;
        input->caret_upstream = false;
        input->blink_s = 0.0;
        input->preferred_x_valid = false;
    } break;
    case app_text_command_copy: {
        if (has_selection) {
            ui_input_post_clipboard(input->buffer + sel_start, sel_end - sel_start);
        }
    } break;
    case app_text_command_cut: {
        if (has_selection) {
            ui_input_post_clipboard(input->buffer + sel_start, sel_end - sel_start);
            ui_text_input_delete_range(input, sel_start, sel_end);
        }
    } break;
    case app_text_command_delete_line: {
        uint32_t line_start;
        uint32_t line_end;
        uint32_t unused;
        ui_text_input_paragraph_range(input, sel_start, &line_start, &unused);
        ui_text_input_paragraph_range(input, sel_end, &unused, &line_end);
        if (line_end == input->size && line_start > 0 && input->buffer[line_start - 1] == '\n') {
            line_start--;
        }
        ui_text_input_delete_range(input, line_start, line_end);
    } break;
    case app_text_command_delete_line_start: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else {
            uint32_t line_start;
            uint32_t line_end;
            ui_text_input_line_range(input, input->caret, input->caret_upstream, &line_start, &line_end);
            ui_text_input_delete_range(input, line_start, input->caret);
        }
    } break;
    case app_text_command_delete_line_end: {
        if (has_selection) {
            ui_text_input_delete_range(input, sel_start, sel_end);
        } else {
            uint32_t line_start;
            uint32_t line_end;
            ui_text_input_line_range(input, input->caret, input->caret_upstream, &line_start, &line_end);
            if (input->caret < line_end) {
                ui_text_input_delete_range(input, input->caret, line_end);
            } else if (input->caret < input->size && input->buffer[input->caret] == '\n') {
                ui_text_input_delete_range(input, input->caret, input->caret + 1);
            }
        }
    } break;
    }
}

static void ui_text_input_on_event(const struct app_event* event, void* user) {
    struct ui_text_input* input = user;

    if (input->has_layout == false) {
        return;
    }

    switch (event->type) {
    case app_event_type_text: {
        ui_text_input_insert(input, event->text.utf8, event->text.size);
    } break;
    case app_event_type_marked_text: {
        ui_text_input_handle_marked(input, &event->marked_text);
    } break;
    case app_event_type_text_command: {
        ui_text_input_handle_command(input, event->text_command.command, event->text_command.select);
    } break;
    case app_event_type_floating_caret: {
        if (event->floating_caret.phase == app_floating_caret_phase_ended || ui_node_is_null(input->prev_main)) {
            break;
        }
        bool upstream = false;
        uint32_t offset = ui_text_input_offset_at(input, ui_text_input_text_pos(input, event->floating_caret.pos), &upstream);
        ui_text_input_move_caret(input, offset, false, false);
        input->caret_upstream = upstream;
    } break;
    default: break;
    }
}

static double2 ui_text_input_text_pos(struct ui_text_input* input, double2 root_pos) {
    double2 local = ui_to_local_from_root(input->prev_root, input->prev_main, root_pos);
    return local - input->applied_content_pos;
}

static void ui_text_input_drag_update(struct ui_text_input* input, double2 root_pos) {
    bool upstream = false;
    uint32_t offset = ui_text_input_offset_at(input, ui_text_input_text_pos(input, root_pos), &upstream);

    if (input->click_count >= 2) {
        uint32_t range_start;
        uint32_t range_end;
        if (input->click_count >= 3) {
            ui_text_input_paragraph_range(input, offset, &range_start, &range_end);
        } else {
            ui_text_input_word_range(input, offset, &range_start, &range_end);
        }

        if (range_start < input->drag_origin_start) {
            input->anchor = input->drag_origin_end;
            input->caret = range_start;
        } else if (range_end > input->drag_origin_end) {
            input->anchor = input->drag_origin_start;
            input->caret = range_end;
        } else {
            input->anchor = input->drag_origin_start;
            input->caret = input->drag_origin_end;
        }
        input->caret_upstream = false;
    } else {
        input->caret = offset;
        input->caret_upstream = upstream;
        if (input->drag_caret_only) {
            input->anchor = input->caret;
        }
    }
    input->blink_s = 0.0;
}

static uint32_t ui_text_input_touch_snap_offset(struct ui_text_input* input, uint32_t offset, double tap_x) {
    if (offset == 0 || offset >= input->size) {
        return offset;
    }

    uint32_t size = (uint32_t)input->size;
    uint32_t next;
    uint32_t prev;
    uint32_t kind_after = ui_text_input_char_kind(ui_text_input_decode_at(input->buffer, size, offset, &next));
    uint32_t kind_before = ui_text_input_char_kind(ui_text_input_decode_before(input->buffer, size, offset, &prev));

    if (kind_after != ui_text_input_char_kind_word || kind_before != ui_text_input_char_kind_word) {
        return offset;
    }

    uint32_t start;
    uint32_t end;
    ui_text_input_word_range(input, offset, &start, &end);
    if (start >= offset || end <= offset) {
        return offset;
    }

    struct ui_text_edit_rect start_rect = ui_text_input_caret_rect(input, start, false);
    struct ui_text_edit_rect end_rect = ui_text_input_caret_rect(input, end, true);
    if (fabs(start_rect.y - end_rect.y) > 0.5) {
        return offset;
    }
    return fabs(tap_x - start_rect.x) <= fabs(tap_x - end_rect.x) ? start : end;
}

static void ui_text_input_touch_tap(struct ui_text_input* input, struct app_pointer_event event) {
    bool is_multi_tap = event.timestamp - input->last_press_timestamp <= ui_text_input_multi_click_interval_s &&
                        fabs(event.pos.x - input->last_press_pos.x) <= ui_text_input_touch_slop_pt &&
                        fabs(event.pos.y - input->last_press_pos.y) <= ui_text_input_touch_slop_pt;

    if (is_multi_tap == false && ui_text_input_handle_hit(input, event.pos) != ui_text_input_handle_none) {
        input->click_count = 1;
        input->last_press_timestamp = event.timestamp;
        input->last_press_pos = event.pos;
        return;
    }

    input->touch_selection = true;

    double2 pos = ui_text_input_text_pos(input, event.pos);
    bool upstream = false;
    uint32_t offset = ui_text_input_offset_at(input, pos, &upstream);

    input->click_count = is_multi_tap ? min_uint32(input->click_count + 1, 3) : 1;
    input->last_press_timestamp = event.timestamp;
    input->last_press_pos = event.pos;

    if (input->click_count == 2) {
        uint32_t start;
        uint32_t end;
        ui_text_input_word_range(input, offset, &start, &end);
        input->anchor = start;
        input->caret = end;
        input->caret_upstream = false;
    } else if (input->click_count >= 3) {
        uint32_t start;
        uint32_t end;
        ui_text_input_paragraph_range(input, offset, &start, &end);
        input->anchor = start;
        input->caret = end;
        input->caret_upstream = false;
    } else {
        uint32_t snapped = ui_text_input_touch_snap_offset(input, offset, pos.x);
        if (snapped != offset) {
            offset = snapped;
            upstream = ui_text_input_is_soft_boundary(input, snapped);
        }
        input->anchor = offset;
        input->caret = offset;
        input->caret_upstream = upstream;
    }

    input->blink_s = 0.0;
    input->preferred_x_valid = false;
    input->has_marked = false;
    input->marked_size = 0;
    ui_input_set_focus(input->focus_key);
}

static bool ui_text_input_handle_rects(struct ui_text_input* input, struct ui_text_edit_rect* out_start, struct ui_text_edit_rect* out_end) {
    uint32_t sel_start = ui_text_input_sel_start(input);
    uint32_t sel_end = ui_text_input_sel_end(input);

    if (input->touch_selection == false || sel_start == sel_end || ui_text_input_is_focused(input) == false) {
        return false;
    }

    *out_start = ui_text_input_caret_rect(input, sel_start, false);
    *out_end = ui_text_input_caret_rect(input, sel_end, ui_text_input_is_soft_boundary(input, sel_end));
    return true;
}

static uint32_t ui_text_input_handle_hit(struct ui_text_input* input, double2 root_pos) {
    struct ui_text_edit_rect start_rect;
    struct ui_text_edit_rect end_rect;
    bool has_handles = ui_text_input_handle_rects(input, &start_rect, &end_rect);

    if (has_handles == false) {
        return ui_text_input_handle_none;
    }

    double2 pos = ui_text_input_text_pos(input, root_pos);
    double2 start_knob = make_double2(start_rect.x, start_rect.y - ui_text_input_handle_knob_offset_pt);
    double2 end_knob = make_double2(end_rect.x, end_rect.y + end_rect.height + ui_text_input_handle_knob_offset_pt);

    double start_dist = max_double(fabs(pos.x - start_knob.x), fabs(pos.y - start_knob.y));
    double end_dist = max_double(fabs(pos.x - end_knob.x), fabs(pos.y - end_knob.y));

    if (start_dist <= ui_text_input_handle_grab_pt || end_dist <= ui_text_input_handle_grab_pt) {
        return start_dist <= end_dist ? ui_text_input_handle_start : ui_text_input_handle_end;
    }
    return ui_text_input_handle_none;
}

static void ui_text_input_scroll_pan(struct ui_text_input* input, struct ui_pan_gesture pan) {
    if (ui_scroll_on_pan(&input->scroll, pan, ui_pan_direction_y)) {
        ui_pan_gesture_claim(pan);
    }
}

static void ui_text_input_on_pan(struct ui_pan_gesture pan, void* user) {
    struct ui_text_input* input = user;

    enum ui_gesture_phase phase = ui_pan_gesture_get_phase(pan);
    double2 pos = ui_pan_gesture_get_position(pan);

    if (phase == ui_gesture_phase_possible || phase == ui_gesture_phase_failed) {
        if (input->last_layout.multiline) {
            ui_text_input_scroll_pan(input, pan);
        }
        return;
    }

    if (phase == ui_gesture_phase_began) {
        input->pan_handle = ui_text_input_handle_hit(input, pos - ui_pan_gesture_get_translation(pan, true));

        if (input->pan_handle != ui_text_input_handle_none) {
            uint32_t sel_start = ui_text_input_sel_start(input);
            uint32_t sel_end = ui_text_input_sel_end(input);

            input->anchor = input->pan_handle == ui_text_input_handle_start ? sel_end : sel_start;
            input->caret = input->pan_handle == ui_text_input_handle_start ? sel_start : sel_end;
            input->caret_upstream = false;
            input->click_count = 1;
            input->drag_origin_start = input->anchor;
            input->drag_origin_end = input->anchor;
            input->is_dragging = true;
            input->drag_caret_only = false;
            input->last_drag_pos = pos;
            input->blink_s = 0.0;
            input->preferred_x_valid = false;
            return;
        }
    }

    if (input->pan_handle != ui_text_input_handle_none) {
        if (phase == ui_gesture_phase_changed) {
            input->last_drag_pos = pos;
            ui_text_input_drag_update(input, pos);
        } else if (phase == ui_gesture_phase_ended || phase == ui_gesture_phase_canceled) {
            input->pan_handle = ui_text_input_handle_none;
            input->is_dragging = false;
        }
        return;
    }

    if (input->last_layout.multiline) {
        ui_text_input_scroll_pan(input, pan);
    } else {
        if (phase != ui_gesture_phase_began) {
            input->scroll_x -= pos.x - input->pan_last_x;
        }
        input->pan_last_x = pos.x;
    }
}

static void ui_text_input_on_long_press(struct ui_long_press_gesture gesture, void* user) {
    struct ui_text_input* input = user;

    if (input->has_layout == false || ui_node_is_null(input->prev_main)) {
        return;
    }

    double2 pos = ui_long_press_gesture_get_position(gesture);

    switch (ui_long_press_gesture_get_phase(gesture)) {
    case ui_gesture_phase_began: {
        if (ui_text_input_handle_hit(input, pos) != ui_text_input_handle_none) {
            return;
        }

        bool upstream = false;
        uint32_t offset = ui_text_input_offset_at(input, ui_text_input_text_pos(input, pos), &upstream);

        input->click_count = 1;
        input->last_press_timestamp = -1.0e9;
        input->drag_origin_start = offset;
        input->drag_origin_end = offset;
        input->anchor = offset;
        input->caret = offset;
        input->caret_upstream = upstream;
        input->is_dragging = true;
        input->drag_caret_only = true;
        input->touch_selection = true;
        input->last_drag_pos = pos;
        input->blink_s = 0.0;
        input->preferred_x_valid = false;
        input->has_marked = false;
        input->marked_size = 0;
        ui_input_set_focus(input->focus_key);
    } break;
    case ui_gesture_phase_changed: {
        if (input->is_dragging) {
            input->last_drag_pos = pos;
            ui_text_input_drag_update(input, pos);
        }
    } break;
    case ui_gesture_phase_ended:
    case ui_gesture_phase_canceled: {
        input->is_dragging = false;
        input->drag_caret_only = false;
    } break;
    default: break;
    }
}

static void ui_text_input_on_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
    struct ui_text_input* input = user;

    if (input->has_layout == false || ui_node_is_null(input->prev_main)) {
        return;
    }

    switch (event.type) {
    case app_pointer_event_type_touch_began: {
        if (event.source == app_pointer_source_direct) {
            break;
        }
        input->touch_selection = false;

        bool upstream = false;
        uint32_t offset = ui_text_input_offset_at(input, ui_text_input_text_pos(input, event.pos), &upstream);

        bool extend = (event.modifiers & app_modifier_shift) != 0 && ui_text_input_is_focused(input);

        bool is_multi_click = extend == false &&
                              event.timestamp - input->last_press_timestamp <= ui_text_input_multi_click_interval_s &&
                              fabs(event.pos.x - input->last_press_pos.x) <= ui_text_input_multi_click_slop_pt &&
                              fabs(event.pos.y - input->last_press_pos.y) <= ui_text_input_multi_click_slop_pt;
        input->click_count = is_multi_click ? min_uint32(input->click_count + 1, 3) : 1;
        input->last_press_timestamp = event.timestamp;
        input->last_press_pos = event.pos;

        if (extend) {
            input->drag_origin_start = input->anchor;
            input->drag_origin_end = input->anchor;
            input->caret = offset;
            input->caret_upstream = upstream;
        } else {
            if (input->click_count == 2) {
                ui_text_input_word_range(input, offset, &input->drag_origin_start, &input->drag_origin_end);
            } else if (input->click_count >= 3) {
                ui_text_input_paragraph_range(input, offset, &input->drag_origin_start, &input->drag_origin_end);
            } else {
                input->drag_origin_start = offset;
                input->drag_origin_end = offset;
            }

            input->anchor = input->drag_origin_start;
            input->caret = input->drag_origin_end;
            input->caret_upstream = input->click_count == 1 ? upstream : false;
        }

        input->is_dragging = true;
        input->drag_caret_only = false;
        input->last_drag_pos = event.pos;
        input->blink_s = 0.0;
        input->preferred_x_valid = false;
        input->has_marked = false;
        input->marked_size = 0;
        ui_input_set_focus(input->focus_key);
    } break;
    case app_pointer_event_type_touch_moved: {
        if (input->is_dragging) {
            input->last_drag_pos = event.pos;
            ui_text_input_drag_update(input, event.pos);
        }
    } break;
    case app_pointer_event_type_touch_ended: {
        if (event.source == app_pointer_source_direct && input->is_dragging == false) {
            ui_text_input_touch_tap(input, event);
        }
        input->is_dragging = false;
        input->drag_caret_only = false;
    } break;
    case app_pointer_event_type_canceled: {
        if (input->drag_caret_only == false && input->pan_handle == ui_text_input_handle_none) {
            input->is_dragging = false;
        }
    } break;
    default: break;
    }
}

static void ui_text_input_on_scroll(struct app_scroll_event event, void* user) {
    struct ui_text_input* input = user;
    if (input->last_layout.multiline) {
        ui_scroll_on_wheel(&input->scroll, event.delta.y);
    } else {
        input->scroll_x += event.delta.x;
    }
}

struct ui_text_input_on_update_result ui_text_input_on_update(struct ui_text_input* input, struct ui_node root, struct ui_node container, struct ui_text_input_layout layout, double delta_s) {
    if (layout.pt_size <= 0.0f) {
        layout.pt_size = 13.0f;
    }
    if (layout.weight <= 0.0f) {
        layout.weight = 400.0f;
    }

    layout.padding.x = ui_round_px(layout.padding.x);
    layout.padding.y = ui_round_px(layout.padding.y);
    layout.padding.z = ui_round_px(layout.padding.z);
    layout.padding.w = ui_round_px(layout.padding.w);

    if (layout.pt_size != input->last_layout.pt_size ||
        layout.weight != input->last_layout.weight ||
        layout.font != input->last_layout.font ||
        layout.multiline != input->last_layout.multiline ||
        ui_text_input_text_width(&layout) != ui_text_input_text_width(&input->last_layout)) {
        input->scratch_valid = false;
    }

    input->last_layout = layout;
    input->has_layout = true;

    double pad_left = (double)layout.padding.x;
    double pad_top = (double)layout.padding.z;
    double pad_bottom = (double)layout.padding.w;
    double text_width = ui_text_input_text_width(&layout);

    double text_height = ui_text_input_lines_height(input);

    double height = text_height + pad_top + pad_bottom;
    if (layout.max_height > 0.0 && height > layout.max_height) {
        height = layout.max_height;
    }
    if (height < layout.min_height) {
        height = layout.min_height;
    }
    height = ui_round_px(height);

    double view_height = max_double(height - pad_top - pad_bottom, 1.0);
    bool has_overflow = text_height > view_height + 0.5;

    input->last_view_height = view_height;
    input->last_text_height = text_height;

    double content_width = text_width;
    if (layout.multiline == false) {
        content_width = max_double(text_width, ui_text_input_lines_width(input) + ui_text_input_caret_slack_pt);
    }
    bool has_overflow_x = content_width > text_width + 0.5;

    struct ui_node main = ui_node(container, ui_key_u64((uint64_t)(uintptr_t)input));
    input->focus_key = ui_get_key(main);
    if (input->focus_pending) {
        ui_input_set_focus(input->focus_key);
        input->focus_pending = false;
    }

    bool is_focused = ui_text_input_is_focused(input);

    if (is_focused == false) {
        input->is_dragging = false;
        input->has_marked = false;
        input->marked_size = 0;
    }

    if (input->is_dragging && ui_node_is_null(input->prev_main) == false) {
        double2 local = ui_to_local_from_root(input->prev_root, input->prev_main, input->last_drag_pos);
        double over_y = 0.0;
        double over_x = 0.0;

        if (has_overflow) {
            double view_y = local.y - pad_top;
            over_y = view_y < 0.0 ? view_y : (view_y > view_height ? view_y - view_height : 0.0);
        }
        if (has_overflow_x) {
            double view_x = local.x - pad_left;
            over_x = view_x < 0.0 ? view_x : (view_x > text_width ? view_x - text_width : 0.0);
        }

        if (over_y != 0.0) {
            input->scroll.offset = clamp_double(input->scroll.offset + over_y * ui_text_input_autoscroll_per_s * delta_s, 0.0, text_height - view_height);
            input->scroll.velocity = 0.0;
        }
        if (over_x != 0.0) {
            input->scroll_x += over_x * ui_text_input_autoscroll_per_s * delta_s;
        }
        if (over_y != 0.0 || over_x != 0.0) {
            ui_text_input_drag_update(input, input->last_drag_pos);
        }
    }

    uint32_t sel_start = ui_text_input_sel_start(input);
    uint32_t sel_end = ui_text_input_sel_end(input);

    struct ui_text_edit_rect caret_rect = ui_text_input_caret_rect(input, input->caret, input->caret_upstream);

    if (input->scroll_to_caret) {
        input->scroll_to_caret = false;
        if (caret_rect.y + caret_rect.height > input->scroll.offset + view_height) {
            input->scroll.offset = caret_rect.y + caret_rect.height - view_height;
            input->scroll.velocity = 0.0;
        }
        if (caret_rect.y < input->scroll.offset) {
            input->scroll.offset = caret_rect.y;
            input->scroll.velocity = 0.0;
        }
        if (layout.multiline == false) {
            if (caret_rect.x + ui_text_input_caret_slack_pt > input->scroll_x + text_width) {
                input->scroll_x = caret_rect.x + ui_text_input_caret_slack_pt - text_width;
            }
            if (caret_rect.x < input->scroll_x) {
                input->scroll_x = caret_rect.x;
            }
        }
    }

    input->scroll_x = clamp_double(input->scroll_x, 0.0, max_double(content_width - text_width, 0.0));

    ui_scroll_on_update(&input->scroll, input->scroll.offset, input->scroll.offset + view_height, 0.0, text_height, delta_s);

    input->blink_s += delta_s;

    ui_frame(&(struct ui_frame) {
        .node = main,
        .size = ui_size_fix(layout.width, height),
        .clip = true,
    });

    ui_cursor(&(struct ui_cursor) {
        .node = main,
        .cursor = app_cursor_text,
    });
    ui_on_pointer(&(struct ui_on_pointer) {
        .node = main,
        .func = ui_text_input_on_pointer,
        .user = input,
    });
    ui_on_text(&(struct ui_on_text) {
        .node = main,
        .func = ui_text_input_on_event,
        .user = input,
    });
    ui_pan(&(struct ui_pan) {
        .node = main,
        .on_change = ui_text_input_on_pan,
        .user = input,
        .direction = layout.multiline ? ui_pan_direction_any : ui_pan_direction_x,
        .direct_only = true,
        .holds_touches = true,
    });
    ui_long_press(&(struct ui_long_press) {
        .node = main,
        .on_change = ui_text_input_on_long_press,
        .user = input,
        .duration = ui_text_input_long_press_s,
        .direct_only = true,
    });
    if (has_overflow || has_overflow_x) {
        ui_on_scroll(&(struct ui_on_scroll) {
            .node = main,
            .func = ui_text_input_on_scroll,
            .user = input,
        });
    }

    input->applied_content_pos = make_double2(
        pad_left - ui_round_px(input->scroll_x),
        pad_top - ui_scroll_get_offset(&input->scroll));

    struct ui_node content = ui_frame(&(struct ui_frame) {
        .parent = main,
        .pos = ui_pos_xy(input->applied_content_pos.x, input->applied_content_pos.y),
        .size = ui_size_fix(content_width, text_height),
    });

    struct ui_text_input_on_update_result result = { 0 };
    result.main = main;
    result.content = content;
    result.text = input->buffer;
    result.text_size = (uint32_t)input->size;
    result.is_focused = is_focused;
    result.caret = caret_rect;
    result.caret_visible = is_focused && sel_start == sel_end && (fmod(input->blink_s, ui_text_input_blink_period_s) < ui_text_input_blink_on_s || input->is_dragging);
    result.height = height;
    result.content_width = content_width;
    result.content_height = text_height;
    result.scroll_offset = input->scroll.offset;
    result.scroll_offset_x = input->scroll_x;

    uint32_t line_count = ui_text_get_line_count(ui_text_input_text_node(input));

    if (is_focused && sel_start != sel_end) {
        ui_text_input_reserve_rects(input, &input->selection_rects, &input->selection_rect_capacity, line_count);
        result.selection_rect_count = ui_text_input_range_rects(input, sel_start, sel_end, input->selection_rects, input->selection_rect_capacity);
        result.selection_rects = input->selection_rects;

        result.handles_visible = ui_text_input_handle_rects(input, &result.handle_start, &result.handle_end);
    }

    if (input->has_marked && input->marked_size > 0) {
        ui_text_input_reserve_rects(input, &input->marked_rects, &input->marked_rect_capacity, line_count);
        result.marked_rect_count = ui_text_input_range_rects(input, input->marked_start, input->marked_start + input->marked_size, input->marked_rects, input->marked_rect_capacity);
        result.marked_rects = input->marked_rects;
    }

    if (is_focused && ui_node_is_null(input->prev_main) == false) {
        double2 prev_origin = ui_get_origin_from_root(input->prev_root, input->prev_main);
        double2 caret_origin = prev_origin + input->applied_content_pos + make_double2(caret_rect.x, caret_rect.y);
        ui_input_set_caret_rect(caret_origin, make_double2(2.0, caret_rect.height));
    }

    input->prev_root = root;
    input->prev_main = main;

    return result;
}
