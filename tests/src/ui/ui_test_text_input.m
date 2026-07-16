#include "ui_tests.h"
#include "ui_text_input.h"
#include "ui_core.h"
#include <stdio.h>


static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("text input", ok, label);
}

struct ui_test_input_ctx {
    struct ui_node prev_root;
};

static struct ui_text_input_on_update_result ui_test_input_frame(struct ui_test_input_ctx* ctx, struct ui_text_input* input, const struct ui_text_input_layout* layout, double timestamp, const struct app_event* events, uint32_t event_count) {
    ui_input_dispatch(ctx->prev_root, timestamp, events, event_count, 1.0);

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(600.0, 500.0),
    });

    struct ui_text_input_on_update_result result = ui_text_input_on_update(input, root, root, *layout, 1.0 / 60.0);

    ui_layout(root);

    if (ui_node_is_null(ctx->prev_root) == false) {
        ui_release(ctx->prev_root);
    }
    ctx->prev_root = root;

    return result;
}

static void ui_test_input_end(struct ui_test_input_ctx* ctx, struct ui_text_input* input) {
    ui_input_set_focus(0);
    ui_text_input_destroy(input);
    if (ui_node_is_null(ctx->prev_root) == false) {
        ui_release(ctx->prev_root);
        ctx->prev_root = ui_node_null;
    }
}

static struct app_event ui_test_command_event(enum app_text_command command, bool select) {
    return (struct app_event) {
        .type = app_event_type_text_command,
        .text_command = { .command = command, .select = select },
    };
}

struct app_event ui_test_touch_event_(enum app_pointer_event_type type, double x, double y, double timestamp, uint32_t modifiers) {
    return (struct app_event) {
        .type = app_event_type_pointer,
        .pointer = {
            .type = type,
            .source = app_pointer_source_indirect,
            .id = 91,
            .modifiers = modifiers,
            .pos = make_double2(x, y),
            .timestamp = timestamp,
        },
    };
}

static struct app_event ui_test_text_event(const char* utf8) {
    struct app_event event = {
        .type = app_event_type_text,
        .text.utf8 = utf8,
        .text.size = (uint32_t)strlen(utf8),
    };
    return event;
}

static void ui_test_text_input_vertical_collapse(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "alpha\nbravo\ncharlie";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    struct app_event select_all = ui_test_command_event(app_text_command_select_all, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &select_all, 1);
    struct app_event up = ui_test_command_event(app_text_command_move_up, false);
    ui_test_input_frame(&ctx, input, &layout, 1.2, &up, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.3, NULL, 0);
    ui_test_check(result.caret.y == 0.0 && result.caret.x == 0.0 && result.selection_rect_count == 0, "up collapses to selection start");

    ui_test_input_frame(&ctx, input, &layout, 1.4, &select_all, 1);
    struct app_event down = ui_test_command_event(app_text_command_move_down, false);
    ui_test_input_frame(&ctx, input, &layout, 1.5, &down, 1);
    struct ui_text_input_on_update_result result_down = ui_test_input_frame(&ctx, input, &layout, 1.6, NULL, 0);
    ui_test_check(result_down.caret.y > result.caret.y && result_down.selection_rect_count == 0, "down collapses to selection end");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_affinity_pointer(void) {
    double2 full = ui_test_measure_text_("go gopher", 0.0);

    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = (double)full.x - 1.0 + 16.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    ui_text_input_set_text(input, "go gopher", 9);
    ui_text_input_set_focus(input, true);

    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    double line_height = result.caret.height;
    ui_test_check(line_height > 0.0 && result.caret.y > 0.0, "wraps for affinity test");

    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    struct app_event line_end = ui_test_command_event(app_text_command_move_line_end, false);
    ui_test_input_frame(&ctx, input, &layout, 1.2, &line_end, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.3, NULL, 0);
    ui_test_check(result.caret.y == 0.0 && result.caret.x > 0.0, "end stays on wrapped line");

    struct app_event right = ui_test_command_event(app_text_command_move_right, false);
    ui_test_input_frame(&ctx, input, &layout, 1.4, &right, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.5, NULL, 0);
    ui_test_check(result.caret.y > 0.0, "right crosses wrap boundary");

    double click_x = 8.0 + (double)full.x - 5.0;
    double line0_y = 6.0 + line_height * 0.5;
    struct app_event press = ui_test_touch_event_(app_pointer_event_type_touch_began, click_x, line0_y, 2.0, 0);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &press, 1);
    struct app_event release = ui_test_touch_event_(app_pointer_event_type_touch_ended, click_x, line0_y, 2.05, 0);
    ui_test_input_frame(&ctx, input, &layout, 2.05, &release, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 2.1, NULL, 0);
    ui_test_check(result.caret.y == 0.0 && result.caret.x > 0.0 && result.selection_rect_count == 0, "click past wrap end is upstream");

    double line1_y = 6.0 + line_height * 1.5;
    struct app_event shift_press = ui_test_touch_event_(app_pointer_event_type_touch_began, click_x, line1_y, 3.0, app_modifier_shift);
    ui_test_input_frame(&ctx, input, &layout, 3.0, &shift_press, 1);
    struct app_event shift_release = ui_test_touch_event_(app_pointer_event_type_touch_ended, click_x, line1_y, 3.05, app_modifier_shift);
    ui_test_input_frame(&ctx, input, &layout, 3.05, &shift_release, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 3.1, NULL, 0);
    ui_test_check(result.selection_rect_count == 1, "shift click extends selection");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_single_line(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 120.0,
        .min_height = 24.0,
        .max_height = 24.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 5.0, 5.0),
        .multiline = false,
    };
    double text_width = 120.0 - 16.0;

    const char* text = "one two three four five six seven";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    ui_test_check(result.content_height < 25.0, "single line no wrap");
    ui_test_check(result.content_width > text_width, "single line overflows");
    ui_test_check(result.scroll_offset_x > 0.0, "single line scrolls to caret");
    ui_test_check(fabs(result.caret.x + 2.0 - (result.scroll_offset_x + text_width)) < 0.5, "caret at right edge");

    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.2, NULL, 0);
    ui_test_check(result.scroll_offset_x == 0.0 && result.caret.x == 0.0, "doc start resets scroll");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_drag_autoscroll(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 200.0,
        .max_height = 60.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(6.0, 6.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.2, NULL, 0);
    ui_test_check(result.scroll_offset == 0.0 && result.content_height > 60.0, "autoscroll baseline");

    struct app_event press = ui_test_touch_event_(app_pointer_event_type_touch_began, 10.0, 10.0, 2.0, 0);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &press, 1);
    struct app_event drag = ui_test_touch_event_(app_pointer_event_type_touch_moved, 10.0, 300.0, 2.02, 0);
    ui_test_input_frame(&ctx, input, &layout, 2.02, &drag, 1);

    double timestamp = 2.04;
    for (uint32_t i = 0; i < 20; i++) {
        result = ui_test_input_frame(&ctx, input, &layout, timestamp, NULL, 0);
        timestamp += 1.0 / 60.0;
    }

    double view_height = 60.0 - 12.0;
    ui_test_check(fabs(result.scroll_offset - (result.content_height - view_height)) < 0.5, "autoscroll reaches end");
    ui_test_check(result.selection_rect_count == 10, "autoscroll selects to end");
    ui_test_check(result.caret.y > result.content_height - 20.0, "autoscroll caret at end");

    struct app_event release = ui_test_touch_event_(app_pointer_event_type_touch_ended, 10.0, 300.0, timestamp, 0);
    ui_test_input_frame(&ctx, input, &layout, timestamp, &release, 1);

    ui_test_input_end(&ctx, input);
}

static uint32_t ui_test_input_text_size(struct ui_text_input* input) {
    const char* text;
    uint64_t size;
    ui_text_input_get_text(input, &text, &size);
    return (uint32_t)size;
}

static void ui_test_text_input_trailing_newline(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    ui_text_input_set_text(input, "hello", 5);
    ui_text_input_set_focus(input, true);

    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    double line_height = result.caret.height;
    double single_height = result.content_height;

    struct app_event enter = ui_test_command_event(app_text_command_insert_newline, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &enter, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.2, NULL, 0);
    ui_test_check(fabs(result.content_height - 2.0 * single_height) < 0.5, "enter at end grows field");
    ui_test_check(result.caret.height > 1.0 && result.caret.x == 0.0 && fabs(result.caret.y - line_height) < 0.5, "enter at end caret visible");
    ui_test_check(result.caret_visible, "enter at end caret blink on");

    struct app_event type_x = ui_test_text_event("x");
    ui_test_input_frame(&ctx, input, &layout, 1.3, &type_x, 1);
    struct app_event backspace = ui_test_command_event(app_text_command_delete_backward, false);
    ui_test_input_frame(&ctx, input, &layout, 1.4, &backspace, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.5, NULL, 0);
    ui_test_check(ui_test_input_text_size(input) == 6 && result.caret.height > 1.0 && fabs(result.caret.y - line_height) < 0.5, "delete back to trailing newline");

    ui_test_input_frame(&ctx, input, &layout, 1.6, &backspace, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.7, NULL, 0);
    ui_test_check(fabs(result.content_height - single_height) < 0.5 && result.caret.y == 0.0 && result.caret.x > 0.0, "delete newline rejoins line");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_delete_line(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "alpha\nbravo\ncharlie";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    struct app_event down = ui_test_command_event(app_text_command_move_down, false);
    struct app_event delete_line = ui_test_command_event(app_text_command_delete_line, false);

    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    ui_test_input_frame(&ctx, input, &layout, 1.2, &down, 1);
    ui_test_input_frame(&ctx, input, &layout, 1.3, &delete_line, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.4, NULL, 0);
    double line_height = result.caret.height;
    ui_test_check(ui_test_input_text_size(input) == 13 && result.caret.x == 0.0 && fabs(result.caret.y - line_height) < 0.5 && line_height > 1.0, "delete middle line");

    ui_test_input_frame(&ctx, input, &layout, 1.5, &delete_line, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.6, NULL, 0);
    ui_test_check(ui_test_input_text_size(input) == 5 && result.caret.y == 0.0 && result.caret.x > 0.0 && result.caret.height > 1.0, "delete last line eats newline");

    ui_test_input_frame(&ctx, input, &layout, 1.7, &delete_line, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.8, NULL, 0);
    ui_test_check(ui_test_input_text_size(input) == 0 && result.caret.x == 0.0 && result.caret.y == 0.0 && result.caret.height > 1.0, "delete only line");

    ui_test_input_end(&ctx, input);
}

static double ui_test_input_caret_x(struct ui_test_input_ctx* ctx, struct ui_text_input* input, const struct ui_text_input_layout* layout, double timestamp) {
    struct ui_text_input_on_update_result result = ui_test_input_frame(ctx, input, layout, timestamp, NULL, 0);
    return result.caret.x;
}

static void ui_test_text_input_word_boundaries(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 400.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "abc\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97""def";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);
    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    double abc = ui_test_text_advance_("abc");
    double abc_cjk = ui_test_text_advance_("abc\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97");

    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    struct app_event word_right = ui_test_command_event(app_text_command_move_word_right, false);
    struct app_event word_left = ui_test_command_event(app_text_command_move_word_left, false);

    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    ui_test_input_frame(&ctx, input, &layout, 1.2, &word_right, 1);
    double x_after_abc = ui_test_input_caret_x(&ctx, input, &layout, 1.3);
    ui_test_check(fabs(x_after_abc - abc) < 0.5, "word right stops before cjk run");

    ui_test_input_frame(&ctx, input, &layout, 1.4, &word_right, 1);
    double x_after_cjk = ui_test_input_caret_x(&ctx, input, &layout, 1.5);
    ui_test_check(fabs(x_after_cjk - abc_cjk) < 0.5, "word right crosses cjk run");

    ui_test_input_frame(&ctx, input, &layout, 1.6, &word_left, 1);
    double x_back = ui_test_input_caret_x(&ctx, input, &layout, 1.7);
    ui_test_check(fabs(x_back - abc) < 0.5, "word left crosses cjk run");

    const char* punct_text = "foo, bar";
    ui_text_input_set_text(input, punct_text, strlen(punct_text));
    struct app_event doc_end = ui_test_command_event(app_text_command_move_doc_end, false);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &doc_end, 1);
    ui_test_input_frame(&ctx, input, &layout, 2.1, &word_left, 1);
    double foo_comma_space = ui_test_text_advance_("foo, ");
    double x_bar = ui_test_input_caret_x(&ctx, input, &layout, 2.2);
    ui_test_check(fabs(x_bar - foo_comma_space) < 0.5, "word left skips to word start");
    ui_test_input_frame(&ctx, input, &layout, 2.3, &word_left, 1);
    double x_foo = ui_test_input_caret_x(&ctx, input, &layout, 2.4);
    ui_test_check(x_foo == 0.0, "word left hops punctuation");

    const char* mark_text = "cafe\xcc\x81 bar";
    ui_text_input_set_text(input, mark_text, strlen(mark_text));
    ui_test_input_frame(&ctx, input, &layout, 3.0, &doc_start, 1);
    struct app_event word_right_select = ui_test_command_event(app_text_command_move_word_right, true);
    ui_test_input_frame(&ctx, input, &layout, 3.1, &word_right_select, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 3.2, NULL, 0);
    double cafe = ui_test_text_advance_("cafe\xcc\x81");
    ui_test_check(result.selection_rect_count == 1 && fabs(result.selection_rects[0].width - cafe) < 0.5, "word selection includes combining mark");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_cjk_double_click(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 400.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "abc\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97""def";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);
    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    double abc = ui_test_text_advance_("abc");
    double abc_cjk = ui_test_text_advance_("abc\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97");

    double click_x = 8.0 + (abc + abc_cjk) * 0.5;
    double click_y = 6.0 + 8.0;

    struct app_event press_a = ui_test_touch_event_(app_pointer_event_type_touch_began, click_x, click_y, 5.0, 0);
    struct app_event release_a = ui_test_touch_event_(app_pointer_event_type_touch_ended, click_x, click_y, 5.05, 0);
    struct app_event press_b = ui_test_touch_event_(app_pointer_event_type_touch_began, click_x, click_y, 5.1, 0);
    struct app_event release_b = ui_test_touch_event_(app_pointer_event_type_touch_ended, click_x, click_y, 5.15, 0);

    ui_test_input_frame(&ctx, input, &layout, 5.0, &press_a, 1);
    ui_test_input_frame(&ctx, input, &layout, 5.05, &release_a, 1);
    ui_test_input_frame(&ctx, input, &layout, 5.1, &press_b, 1);
    ui_test_input_frame(&ctx, input, &layout, 5.15, &release_b, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 5.2, NULL, 0);

    ui_test_check(result.selection_rect_count == 1 &&
                     fabs(result.selection_rects[0].x - abc) < 0.5 &&
                     fabs(result.selection_rects[0].x + result.selection_rects[0].width - abc_cjk) < 0.5,
                     "double click selects cjk run");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_scroll_commands(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 200.0,
        .max_height = 60.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(6.0, 6.0, 6.0, 6.0),
        .multiline = true,
    };
    double view_height = 60.0 - 12.0;

    const char* text = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    struct app_event doc_start = ui_test_command_event(app_text_command_move_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.1, &doc_start, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.2, NULL, 0);
    double max_offset = result.content_height - view_height;

    struct app_event scroll_page_down = ui_test_command_event(app_text_command_scroll_page_down, false);
    ui_test_input_frame(&ctx, input, &layout, 1.3, &scroll_page_down, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.4, NULL, 0);
    ui_test_check(fabs(result.scroll_offset - view_height) < 0.5 && result.caret.y == 0.0 && result.selection_rect_count == 0, "scroll page down keeps caret");

    struct app_event scroll_doc_end = ui_test_command_event(app_text_command_scroll_doc_end, false);
    ui_test_input_frame(&ctx, input, &layout, 1.5, &scroll_doc_end, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.6, NULL, 0);
    ui_test_check(fabs(result.scroll_offset - max_offset) < 0.5 && result.caret.y == 0.0, "scroll doc end keeps caret");

    struct app_event scroll_doc_start = ui_test_command_event(app_text_command_scroll_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.7, &scroll_doc_start, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.8, NULL, 0);
    ui_test_check(result.scroll_offset == 0.0, "scroll doc start");

    struct app_event page_down = ui_test_command_event(app_text_command_move_page_down, false);
    ui_test_input_frame(&ctx, input, &layout, 1.9, &page_down, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 2.0, NULL, 0);
    double line_height = result.caret.height;
    double expected_y = floor((line_height * 0.5 + view_height) / line_height) * line_height;
    ui_test_check(fabs(result.caret.y - expected_y) < 0.5 && result.caret.y >= 2.0 * line_height, "move page down");

    struct app_event page_up_select = ui_test_command_event(app_text_command_move_page_up, true);
    ui_test_input_frame(&ctx, input, &layout, 2.1, &page_up_select, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 2.2, NULL, 0);
    ui_test_check(result.caret.y == 0.0 && result.selection_rect_count >= 3, "move page up selects");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_single_line_scroll_commands(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 120.0,
        .min_height = 24.0,
        .max_height = 24.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 5.0, 5.0),
        .multiline = false,
    };
    double text_width = 120.0 - 16.0;

    const char* text = "one two three four five six seven";
    ui_text_input_set_text(input, text, strlen(text));
    ui_text_input_set_focus(input, true);

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.1, NULL, 0);
    double caret_end_x = result.caret.x;
    double max_scroll_x = result.content_width - text_width;
    ui_test_check(fabs(result.scroll_offset_x - max_scroll_x) < 0.5, "single line at end");

    struct app_event scroll_doc_start = ui_test_command_event(app_text_command_scroll_doc_start, false);
    ui_test_input_frame(&ctx, input, &layout, 1.2, &scroll_doc_start, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.3, NULL, 0);
    ui_test_check(result.scroll_offset_x == 0.0 && result.caret.x == caret_end_x, "single line scroll home keeps caret");

    struct app_event scroll_doc_end = ui_test_command_event(app_text_command_scroll_doc_end, false);
    ui_test_input_frame(&ctx, input, &layout, 1.4, &scroll_doc_end, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 1.5, NULL, 0);
    ui_test_check(fabs(result.scroll_offset_x - max_scroll_x) < 0.5, "single line scroll end");

    ui_test_input_end(&ctx, input);
}

struct app_event ui_test_direct_touch_event_(enum app_pointer_event_type type, double x, double y, double timestamp) {
    struct app_event event = ui_test_touch_event_(type, x, y, timestamp, 0);
    event.pointer.source = app_pointer_source_direct;
    return event;
}

static void ui_test_text_input_touch_tap(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "hello world foo";
    ui_text_input_set_text(input, text, strlen(text));

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    double2 hello_sp = ui_test_measure_text_("hello ", 0.0);
    double2 hello_world = ui_test_measure_text_("hello world", 0.0);
    double2 hello_worl = ui_test_measure_text_("hello worl", 0.0);

    double tap_x = 8.0 + (double)hello_worl.x + 1.0;
    double tap_y = 6.0 + 8.0;

    struct app_event press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, tap_x, tap_y, 2.0);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &press, 1);
    struct app_event release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, tap_x, tap_y, 2.05);
    ui_test_input_frame(&ctx, input, &layout, 2.05, &release, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 2.1, NULL, 0);
    ui_test_check(result.is_focused, "touch tap focuses");
    ui_test_check(fabs(result.caret.x - (double)hello_world.x) < 0.5 && result.selection_rect_count == 0, "touch tap snaps to word edge");

    struct app_event press_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, tap_x, tap_y, 2.2);
    ui_test_input_frame(&ctx, input, &layout, 2.2, &press_b, 1);
    struct app_event release_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, tap_x, tap_y, 2.25);
    ui_test_input_frame(&ctx, input, &layout, 2.25, &release_b, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 2.3, NULL, 0);
    ui_test_check(result.selection_rect_count == 1 &&
                     fabs(result.selection_rects[0].x - (double)hello_sp.x) < 0.5 &&
                     fabs(result.selection_rects[0].x + result.selection_rects[0].width - (double)hello_world.x) < 0.5,
                     "touch double tap selects word");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_touch_pan(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 200.0,
        .max_height = 60.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(6.0, 6.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9";
    ui_text_input_set_text(input, text, strlen(text));

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    struct app_event press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, 100.0, 40.0, 2.0);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &press, 1);
    struct app_event move_a = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 100.0, 26.0, 2.016);
    ui_test_input_frame(&ctx, input, &layout, 2.016, &move_a, 1);
    struct app_event move_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 100.0, 16.0, 2.033);
    ui_test_input_frame(&ctx, input, &layout, 2.033, &move_b, 1);
    struct app_event move_c = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 100.0, 12.0, 2.041);
    ui_test_input_frame(&ctx, input, &layout, 2.041, &move_c, 1);
    struct app_event release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, 100.0, 12.0, 2.05);
    ui_test_input_frame(&ctx, input, &layout, 2.05, &release, 1);

    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 2.1, NULL, 0);
    ui_test_check(result.scroll_offset > 20.0, "touch pan scrolls");
    ui_test_check(result.is_focused == false && result.selection_rect_count == 0, "touch pan does not focus or select");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_touch_long_press(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "alpha\nbravo\ncharlie";
    ui_text_input_set_text(input, text, strlen(text));

    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
    double line_height = result.caret.height;

    double press_x = 8.0 + 10.0;
    double press_y = 6.0 + line_height * 0.5;

    struct app_event press = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, press_x, press_y, 5.0);
    ui_test_input_frame(&ctx, input, &layout, 5.0, &press, 1);

    double timestamp = 5.1;
    for (uint32_t i = 0; i < 6; i++) {
        result = ui_test_input_frame(&ctx, input, &layout, timestamp, NULL, 0);
        timestamp += 0.1;
    }
    ui_test_check(result.is_focused && result.caret.y == 0.0 && result.selection_rect_count == 0, "touch long press places caret");

    struct app_event drag = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, press_x, 6.0 + line_height * 2.5, timestamp);
    ui_test_input_frame(&ctx, input, &layout, timestamp, &drag, 1);
    result = ui_test_input_frame(&ctx, input, &layout, timestamp + 0.016, NULL, 0);
    ui_test_check(fabs(result.caret.y - 2.0 * line_height) < 0.5 && result.selection_rect_count == 0, "touch long press drag moves caret only");

    struct app_event release = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, press_x, 6.0 + line_height * 2.5, timestamp + 0.03);
    ui_test_input_frame(&ctx, input, &layout, timestamp + 0.03, &release, 1);
    result = ui_test_input_frame(&ctx, input, &layout, timestamp + 0.05, NULL, 0);
    ui_test_check(fabs(result.caret.y - 2.0 * line_height) < 0.5, "touch long press release keeps caret");

    ui_test_input_end(&ctx, input);
}

static void ui_test_text_input_selection_handles(void) {
    struct ui_test_input_ctx ctx = { 0 };
    struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
    struct ui_text_input_layout layout = {
        .width = 300.0,
        .pt_size = 13.0,
        .weight = 400.0,
        .padding = make_double4(8.0, 8.0, 6.0, 6.0),
        .multiline = true,
    };

    const char* text = "hello world foo";
    ui_text_input_set_text(input, text, strlen(text));

    ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);

    double2 hello_sp = ui_test_measure_text_("hello ", 0.0);
    double2 hello_world = ui_test_measure_text_("hello world", 0.0);
    double2 full = ui_test_measure_text_("hello world foo", 0.0);

    double tap_x = 8.0 + ((double)hello_sp.x + (double)hello_world.x) * 0.5;
    double tap_y = 6.0 + 8.0;

    struct app_event press_a = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, tap_x, tap_y, 2.0);
    struct app_event release_a = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, tap_x, tap_y, 2.05);
    struct app_event press_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, tap_x, tap_y, 2.1);
    struct app_event release_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, tap_x, tap_y, 2.15);
    ui_test_input_frame(&ctx, input, &layout, 2.0, &press_a, 1);
    ui_test_input_frame(&ctx, input, &layout, 2.05, &release_a, 1);
    ui_test_input_frame(&ctx, input, &layout, 2.1, &press_b, 1);
    ui_test_input_frame(&ctx, input, &layout, 2.15, &release_b, 1);
    struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 2.2, NULL, 0);

    ui_test_check(result.handles_visible &&
                     fabs(result.handle_start.x - (double)hello_sp.x) < 0.5 &&
                     fabs(result.handle_end.x - (double)hello_world.x) < 0.5,
                     "touch selection shows handles");

    double handle_x = 8.0 + (double)hello_world.x;
    double handle_y = 6.0 + result.handle_end.y + result.handle_end.height - 2.0;
    struct app_event grab = ui_test_direct_touch_event_(app_pointer_event_type_touch_began, handle_x, handle_y, 3.0);
    ui_test_input_frame(&ctx, input, &layout, 3.0, &grab, 1);
    struct app_event drag_a = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, handle_x + 14.0, handle_y, 3.016);
    ui_test_input_frame(&ctx, input, &layout, 3.016, &drag_a, 1);
    struct app_event drag_b = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 8.0 + (double)full.x + 4.0, tap_y, 3.033);
    ui_test_input_frame(&ctx, input, &layout, 3.033, &drag_b, 1);
    struct app_event drag_c = ui_test_direct_touch_event_(app_pointer_event_type_touch_moved, 8.0 + (double)full.x + 10.0, tap_y, 3.041);
    ui_test_input_frame(&ctx, input, &layout, 3.041, &drag_c, 1);
    struct app_event drop = ui_test_direct_touch_event_(app_pointer_event_type_touch_ended, 8.0 + (double)full.x + 10.0, tap_y, 3.05);
    ui_test_input_frame(&ctx, input, &layout, 3.05, &drop, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 3.1, NULL, 0);

    ui_test_check(result.handles_visible &&
                     result.selection_rect_count == 1 &&
                     fabs(result.selection_rects[0].x - (double)hello_sp.x) < 0.5 &&
                     fabs(result.selection_rects[0].x + result.selection_rects[0].width - (double)full.x) < 0.5,
                     "touch handle drag extends selection");

    struct app_event mouse_press = ui_test_touch_event_(app_pointer_event_type_touch_began, 8.0 + 2.0, tap_y, 4.0, 0);
    ui_test_input_frame(&ctx, input, &layout, 4.0, &mouse_press, 1);
    struct app_event mouse_drag = ui_test_touch_event_(app_pointer_event_type_touch_moved, 8.0 + (double)hello_sp.x, tap_y, 4.05, 0);
    ui_test_input_frame(&ctx, input, &layout, 4.05, &mouse_drag, 1);
    struct app_event mouse_release = ui_test_touch_event_(app_pointer_event_type_touch_ended, 8.0 + (double)hello_sp.x, tap_y, 4.1, 0);
    ui_test_input_frame(&ctx, input, &layout, 4.1, &mouse_release, 1);
    result = ui_test_input_frame(&ctx, input, &layout, 4.2, NULL, 0);

    ui_test_check(result.selection_rect_count == 1 && result.handles_visible == false, "mouse selection hides handles");

    ui_test_input_end(&ctx, input);
}

static bool ui_test_input_on_px_grid_(double v, double to_px_from_pt) {
    double px = v * to_px_from_pt;
    return fabs(px - round(px)) < 1e-6;
}

static void ui_test_text_input_selection_rects_pixel_grid(void) {
    double saved_scale = ui_get_to_px_from_pt();

    const double scales[] = { 1.0, 4.0 / 3.0, 1.5, 2.0, 8.0 / 3.0, 3.0 };

    bool counts_ok = true;
    bool on_grid = true;
    bool tiles = true;

    for (uint32_t s = 0; s < cr_count(scales); s++) {
        double scale = scales[s];
        ui_set_to_px_from_pt(scale);

        struct ui_test_input_ctx ctx = { 0 };
        struct ui_text_input* input = ui_text_input_create(cr_mem_libc);
        struct ui_text_input_layout layout = {
            .width = 300.0,
            .pt_size = 13.0,
            .weight = 400.0,
            .padding = make_double4(8.0, 8.0, 6.0, 6.0),
            .multiline = true,
        };

        const char* text = "alpha\nbravo\ncharlie\ndelta";
        ui_text_input_set_text(input, text, strlen(text));
        ui_text_input_set_focus(input, true);

        ui_test_input_frame(&ctx, input, &layout, 1.0, NULL, 0);
        struct app_event select_all = ui_test_command_event(app_text_command_select_all, false);
        ui_test_input_frame(&ctx, input, &layout, 1.1, &select_all, 1);
        struct ui_text_input_on_update_result result = ui_test_input_frame(&ctx, input, &layout, 1.2, NULL, 0);

        counts_ok = counts_ok && result.selection_rect_count == 4;

        for (uint32_t i = 0; i < result.selection_rect_count; i++) {
            const struct ui_text_edit_rect* rect = &result.selection_rects[i];
            on_grid = on_grid && ui_test_input_on_px_grid_(rect->y, scale);
            on_grid = on_grid && ui_test_input_on_px_grid_(rect->y + rect->height, scale);

            if (i + 1 < result.selection_rect_count) {
                const struct ui_text_edit_rect* next = &result.selection_rects[i + 1];

                long bottom_px = lround(rect->y * scale) + lround(rect->height * scale);
                long top_px = lround(next->y * scale);
                tiles = tiles && bottom_px == top_px;
            }
        }

        ui_test_input_end(&ctx, input);
    }

    ui_set_to_px_from_pt(saved_scale);

    ui_test_check(counts_ok, "selection rect per line at every scale");
    ui_test_check(on_grid, "selection rect edges on pixel grid");
    ui_test_check(tiles, "selection rects tile without gap or overlap");
}

void ui_test_text_input_(void) {

    ui_test_text_input_vertical_collapse();
    ui_test_text_input_affinity_pointer();
    ui_test_text_input_single_line();
    ui_test_text_input_drag_autoscroll();
    ui_test_text_input_trailing_newline();
    ui_test_text_input_delete_line();
    ui_test_text_input_scroll_commands();
    ui_test_text_input_single_line_scroll_commands();
    ui_test_text_input_word_boundaries();
    ui_test_text_input_cjk_double_click();
    ui_test_text_input_touch_tap();
    ui_test_text_input_touch_pan();
    ui_test_text_input_touch_long_press();
    ui_test_text_input_selection_handles();
    ui_test_text_input_selection_rects_pixel_grid();

}

