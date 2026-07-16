#include "ui_core.h"
#include "ui_atlas.h"
#include "ui_render.h"
#include "ui_color.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

cr_warning_push
cr_warning_ignore_everything
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include <harfbuzz/hb.h>
cr_warning_pop

#define ui_font_axis_count_max 16
#define ui_font_variation_count 16
#define ui_text_default_pt_size 13.0
#define ui_text_default_weight 400.0
#define ui_text_pen_epsilon (1.0 / 1024.0)
#define ui_text_overhang_em 0.5
#define ui_text_unused_bytes_max (8u << 20)

struct ui_font_variation {
    double weight;
    double opsz;
    hb_font_t* hb_font;
    uint64_t used;
};

#define cr_handle ui_font_handle
#include "cr_handle.h"

struct ui_font {
    struct ui_font_handle handle;
    uint32_t id;
    FT_Face ft_face;
    hb_face_t* hb_face;
    uint32_t upem;
    int32_t ascender;
    int32_t descender;
    int32_t height;
    int32_t ink_above_ascender;
    int32_t ink_below_descender;
    uint32_t axis_count;
    uint32_t wght_axis;
    uint32_t opsz_axis;
    double opsz_min;
    double opsz_max;
    FT_Fixed axis_defaults[ui_font_axis_count_max];
    int32_t ft_size_266;
    double ft_weight;
    double ft_opsz;
    struct ui_font_variation variations[ui_font_variation_count];
    uint64_t variation_clock;
};

#define cr_pool ui_font_pool
#define cr_pool_item struct ui_font
#define cr_pool_capacity 32
#define cr_pool_handle ui_font_handle
#include "cr_pool.h"

struct ui_text_span {
    uint32_t start;
    uint32_t end;
    struct ui_font* font;
    double pt_size;
    double weight;
    struct ui_color color;
};

struct ui_run {
    double pt_size;
    double weight;
    struct ui_font* font;
    uint32_t start;
    uint32_t end;
};

struct ui_shaped_glyph {
    int32_t advance;
    uint32_t offset;
    uint16_t glyph_id;
    uint16_t run;
    uint32_t slot_hint;
};

struct ui_line_metrics {
    double ascender_pt;
    double descender_pt;
    double gap_pt;
};

struct ui_shaped_line {
    uint32_t glyph_start;
    uint32_t glyph_end;
    double y_pt;
    double ascender_pt;
    double height_pt;
    double width_pt;
};

struct ui_shaped_lines {
    int32_t wrap_width_266;
    int32_t width_266;
    int32_t height_266;
    uint32_t count;
    struct ui_shaped_line lines[];
};

struct ui_shaped_key {
    uint64_t hash;
    const char* raw;
    const struct ui_run* runs;
    uint32_t raw_size;
    uint32_t run_count;
};

struct ui_shaped {
    struct ui_shaped_key key;
    uint32_t rc;
    uint32_t glyph_count;
    uint64_t bytes;
    struct ui_shaped_glyph* glyphs;
    struct ui_shaped_lines* natural;
    struct ui_shaped_lines* constrained;
    struct ui_shaped* prev;
    struct ui_shaped* next;
};

struct ui_attribute_event {
    uint32_t offset;
    uint32_t order;
    const struct ui_str_attribute* attribute;
    bool is_start;
};

struct ui_text_style {
    struct ui_glyph_key key;
    uint32_t phase_count;
    double scale;
    float4 color;
};

static bool ui_shaped_key_is_equal(const struct ui_shaped_key* a, const struct ui_shaped_key* b) {
    static_assert(sizeof(struct ui_run) == 32, "runs are hashed and compared as bytes");
    return a->hash == b->hash && a->raw_size == b->raw_size && a->run_count == b->run_count &&
           memcmp(a->runs, b->runs, a->run_count * sizeof(a->runs[0])) == 0 && memcmp(a->raw, b->raw, a->raw_size) == 0;
}

#define cr_dict ui_dict_shaped
#define cr_dict_key const struct ui_shaped_key*
#define cr_dict_value struct ui_shaped*
#define cr_dict_is_equal(a, b) ui_shaped_key_is_equal(a, b)
#define cr_dict_hash(key) ((key)->hash)
#include "cr_dict.h"

#define cr_arr ui_arr_attribute_event
#define cr_arr_item struct ui_attribute_event
#include "cr_arr.h"

#define cr_arr ui_arr_attribute_ptr
#define cr_arr_item const struct ui_str_attribute*
#include "cr_arr.h"

#define cr_arr ui_arr_text_span
#define cr_arr_item struct ui_text_span
#include "cr_arr.h"

#define cr_arr ui_arr_run
#define cr_arr_item struct ui_run
#include "cr_arr.h"

#define cr_arr ui_arr_shaped_glyph
#define cr_arr_item struct ui_shaped_glyph
#include "cr_arr.h"

#define cr_arr ui_arr_shaped_line
#define cr_arr_item struct ui_shaped_line
#include "cr_arr.h"

struct ui_mod_text {
    struct cr_mem* mem;
    FT_Library ft_library;
    hb_buffer_t* hb_buf;
    hb_language_t hb_lang;
    uint32_t next_font_id;
    struct ui_font* default_font;
    struct ui_font_pool font_pool;
    struct ui_dict_shaped* cache;
    struct ui_shaped* unused_head;
    struct ui_shaped* unused_tail;
    uint64_t unused_bytes;
    struct ui_arr_attribute_ptr* attributes;
    struct ui_arr_attribute_event* events;
    struct ui_arr_attribute_ptr* active;
    struct ui_arr_text_span* spans;
    struct ui_arr_run* runs;
    struct ui_arr_shaped_glyph* glyphs;
    struct ui_arr_shaped_line* lines;
};

static double ui_font_normalize_weight(const struct ui_font* font, double weight) {
    return font->wght_axis == uint32_max ? 0.0 : weight;
}

static double ui_font_normalize_opsz(const struct ui_font* font, double pt_size) {
    return font->opsz_axis == uint32_max ? 0.0 : clamp_double(pt_size, font->opsz_min, font->opsz_max);
}

static void ui_font_set_ft_coords(const struct ui_font* font, double weight, double opsz) {
    if (font->axis_count == 0) {
        return;
    }

    FT_Fixed coords[ui_font_axis_count_max];
    memcpy(coords, font->axis_defaults, font->axis_count * sizeof(coords[0]));

    if (font->wght_axis != uint32_max) {
        coords[font->wght_axis] = (FT_Fixed)(weight * 65536.0);
    }
    if (font->opsz_axis != uint32_max) {
        coords[font->opsz_axis] = (FT_Fixed)(opsz * 65536.0);
    }

    FT_Set_Var_Design_Coordinates(font->ft_face, font->axis_count, coords);
}

static void ui_font_set_ft_state(struct ui_font* font, int32_t size_266, double weight, double opsz) {
    if (font->ft_size_266 != size_266) {
        FT_Error ft_error = FT_Set_Char_Size(font->ft_face, 0, (FT_F26Dot6)size_266, 72, 72);
        assert(ft_error == 0);
        font->ft_size_266 = size_266;
    }

    if (font->ft_weight != weight || font->ft_opsz != opsz) {
        ui_font_set_ft_coords(font, weight, opsz);
        font->ft_weight = weight;
        font->ft_opsz = opsz;
    }
}

struct ui_glyph_bitmap ui_font_rasterize_glyph(struct ui_font* font, uint32_t glyph_id, uint32_t size_266_px, double weight, double opsz, uint32_t phase_x_64) {
    ui_font_set_ft_state(font, (int32_t)size_266_px, weight, opsz);

    FT_Error ft_error = FT_Load_Glyph(font->ft_face, glyph_id, FT_LOAD_NO_HINTING);
    assert(ft_error == 0);

    FT_GlyphSlot slot = font->ft_face->glyph;

    if (slot->format == FT_GLYPH_FORMAT_OUTLINE && phase_x_64 != 0) {
        FT_Outline_Translate(&slot->outline, (FT_Pos)phase_x_64, 0);
    }

    ft_error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
    assert(ft_error == 0);
    assert(slot->bitmap.pitch >= 0);

    return (struct ui_glyph_bitmap) {
        .offset = make_int2(slot->bitmap_left, -slot->bitmap_top),
        .size = make_uint2(slot->bitmap.width, slot->bitmap.rows),
        .bytes_per_row = (uint32_t)slot->bitmap.pitch,
        .raw = slot->bitmap.buffer,
    };
}

static hb_font_t* ui_font_hb_font(struct ui_font* font, double weight, double opsz) {
    struct ui_font_variation* oldest = &font->variations[0];

    for (uint32_t i = 0; i < ui_font_variation_count; i++) {
        struct ui_font_variation* variation = &font->variations[i];

        if (variation->hb_font != NULL && variation->weight == weight && variation->opsz == opsz) {
            variation->used = ++font->variation_clock;
            return variation->hb_font;
        }
        if (variation->used < oldest->used) {
            oldest = variation;
        }
    }

    if (oldest->hb_font != NULL) {
        hb_font_destroy(oldest->hb_font);
    }

    hb_variation_t variations[2] = {
        { .tag = HB_TAG('w', 'g', 'h', 't'), .value = (float)weight },
        { .tag = HB_TAG('o', 'p', 's', 'z'), .value = (float)opsz },
    };
    hb_font_t* hb_font = hb_font_create(font->hb_face);

    hb_font_set_scale(hb_font, (int)(font->upem << 6), (int)(font->upem << 6));
    hb_font_set_variations(hb_font, variations, cr_count(variations));

    *oldest = (struct ui_font_variation) {
        .weight = weight,
        .opsz = opsz,
        .hb_font = hb_font,
        .used = ++font->variation_clock,
    };

    return hb_font;
}

static void ui_mod_text_forget_font(struct ui* ui, struct ui_font* font);

struct ui_font* ui_font_create(const uint8_t* raw_data, uint64_t data_size) {
    struct ui* ui = ui_ctx();

    struct ui_mod_text* mod = ui->mod_text;
    cr_check(ui_font_pool_get_count(&mod->font_pool) < cr_count(mod->font_pool.items), "the font pool is exhausted: %u fonts are alive; destroy fonts that are no longer needed", (uint32_t)cr_count(mod->font_pool.items));

    FT_Face ft_face = NULL;
    FT_Error ft_error = FT_New_Memory_Face(mod->ft_library, (const FT_Byte*)raw_data, (FT_Long)data_size, 0, &ft_face);
    cr_check(ft_error == 0, "the font data could not be loaded");
    cr_check(ft_face->num_glyphs <= uint16_max, "the font has more than %u glyphs", (uint32_t)uint16_max);
    cr_check(ft_face->ascender >= 0 && ft_face->descender <= 0, "the font has unsupported vertical metrics");

    struct ui_font* font = ui_font_pool_alloc(&mod->font_pool);

    font->id = ++mod->next_font_id;
    font->wght_axis = uint32_max;
    font->opsz_axis = uint32_max;
    font->ft_weight = NAN;
    font->ft_opsz = NAN;
    font->ft_face = ft_face;

    font->upem = font->ft_face->units_per_EM;
    font->ascender = font->ft_face->ascender;
    font->descender = font->ft_face->descender;
    font->height = font->ft_face->height;
    font->ink_above_ascender = max_int32((int32_t)font->ft_face->bbox.yMax - font->ascender, 0);
    font->ink_below_descender = max_int32(font->descender - (int32_t)font->ft_face->bbox.yMin, 0);

    FT_MM_Var* mm_var = NULL;
    if (FT_Get_MM_Var(font->ft_face, &mm_var) == 0) {
        font->axis_count = min_uint32(mm_var->num_axis, ui_font_axis_count_max);

        for (uint32_t i = 0; i < font->axis_count; i++) {
            font->axis_defaults[i] = mm_var->axis[i].def;

            if (mm_var->axis[i].tag == FT_MAKE_TAG('w', 'g', 'h', 't')) {
                font->wght_axis = i;
            } else if (mm_var->axis[i].tag == FT_MAKE_TAG('o', 'p', 's', 'z')) {
                font->opsz_axis = i;
                font->opsz_min = (double)mm_var->axis[i].minimum / 65536.0;
                font->opsz_max = (double)mm_var->axis[i].maximum / 65536.0;
            }
        }

        FT_Done_MM_Var(mod->ft_library, mm_var);
    }

    hb_blob_t* blob = hb_blob_create((const char*)raw_data, (unsigned int)data_size, HB_MEMORY_MODE_READONLY, NULL, NULL);
    font->hb_face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);

    return font;
}

void ui_font_destroy(struct ui_font* font) {
    struct ui* ui = ui_ctx();

    struct ui_mod_text* mod = ui->mod_text;

    ui_mod_text_forget_font(ui, font);
    if (mod->default_font == font) {
        mod->default_font = NULL;
    }

    for (uint32_t i = 0; i < ui_font_variation_count; i++) {
        if (font->variations[i].hb_font != NULL) {
            hb_font_destroy(font->variations[i].hb_font);
        }
    }
    hb_face_destroy(font->hb_face);
    FT_Done_Face(font->ft_face);
    ui_font_pool_dealloc(&mod->font_pool, font);
}

void ui_set_default_font(struct ui_font* font) {
    struct ui* ui = ui_ctx();

    ui->mod_text->default_font = font;
}

struct ui_mod_text* ui_mod_text_create(struct cr_mem* mem) {
    struct ui_mod_text* mod = cr_malloc(mem, sizeof(*mod), 0, cr_loc);

    memset(mod, 0, sizeof(*mod));
    mod->mem = mem;
    ui_font_pool_init(&mod->font_pool);

    FT_Error ft_error = FT_Init_FreeType(&mod->ft_library);
    assert(ft_error == 0);

    mod->hb_buf = hb_buffer_create();
    mod->hb_lang = hb_language_from_string("en", -1);
    mod->cache = ui_dict_shaped_create(mem);
    mod->attributes = ui_arr_attribute_ptr_create(mem);
    mod->events = ui_arr_attribute_event_create(mem);
    mod->active = ui_arr_attribute_ptr_create(mem);
    mod->spans = ui_arr_text_span_create(mem);
    mod->runs = ui_arr_run_create(mem);
    mod->glyphs = ui_arr_shaped_glyph_create(mem);
    mod->lines = ui_arr_shaped_line_create(mem);

    return mod;
}

static void ui_shaped_free(struct ui_mod_text* mod, struct ui_shaped* shaped);

void ui_mod_text_destroy(struct ui_mod_text* mod) {
    struct ui_dict_shaped_iter iter = ui_dict_shaped_iter(mod->cache);
    struct ui_dict_shaped_pair* pair;

    while (ui_dict_shaped_iter_next(&iter, &pair)) {
        ui_shaped_free(mod, pair->value);
    }

    ui_dict_shaped_destroy(mod->cache);
    ui_arr_attribute_ptr_destroy(mod->attributes);
    ui_arr_attribute_event_destroy(mod->events);
    ui_arr_attribute_ptr_destroy(mod->active);
    ui_arr_text_span_destroy(mod->spans);
    ui_arr_run_destroy(mod->runs);
    ui_arr_shaped_glyph_destroy(mod->glyphs);
    ui_arr_shaped_line_destroy(mod->lines);
    hb_buffer_destroy(mod->hb_buf);
    FT_Done_FreeType(mod->ft_library);
    cr_free(mod->mem, mod, cr_loc);
}

static void ui_text_add_attribute(struct ui_mod_text* mod, const struct ui_str_attribute* attribute) {
    if (attribute->type != ui_str_attribute_type_none && attribute->start < attribute->end) {
        ui_arr_attribute_ptr_append(mod->attributes, attribute);
    }
}

static bool ui_text_attributes_are_uniform(struct ui_mod_text* mod, uint32_t raw_size) {
    for (uint32_t i = 0; i < ui_arr_attribute_ptr_get_count(mod->attributes); i++) {
        const struct ui_str_attribute* attribute = ui_arr_attribute_ptr_get(mod->attributes, i);

        if (attribute->start > 0 || attribute->end < raw_size) {
            return false;
        }
    }
    return true;
}

static void ui_text_add_attribute_events(struct ui_mod_text* mod, bool is_start) {
    for (uint32_t i = 0; i < ui_arr_attribute_ptr_get_count(mod->attributes); i++) {
        const struct ui_str_attribute* attribute = ui_arr_attribute_ptr_get(mod->attributes, i);

        ui_arr_attribute_event_append(mod->events, (struct ui_attribute_event) {
            .offset = is_start ? attribute->start : attribute->end,
            .order = i,
            .attribute = attribute,
            .is_start = is_start,
        });
    }
}

static int ui_attribute_event_compare(const void* a, const void* b) {
    const struct ui_attribute_event* x = a;
    const struct ui_attribute_event* y = b;

    if (x->offset != y->offset) {
        return x->offset < y->offset ? -1 : 1;
    }
    return x->order < y->order ? -1 : x->order > y->order;
}

static bool ui_attribute_events_are_sorted(const struct ui_attribute_event* events, uint32_t count) {
    for (uint32_t i = 1; i < count; i++) {
        if (ui_attribute_event_compare(&events[i - 1], &events[i]) > 0) {
            return false;
        }
    }
    return true;
}

static void ui_attribute_event_apply(struct ui_arr_attribute_ptr* active, const struct ui_attribute_event* event) {
    if (event->is_start) {
        ui_arr_attribute_ptr_append(active, event->attribute);
        return;
    }

    uint32_t i = ui_arr_attribute_ptr_get_count(active) - 1;
    while (ui_arr_attribute_ptr_get(active, i) != event->attribute) {
        i--;
    }
    ui_arr_attribute_ptr_remove(active, i);
}

static bool ui_text_span_has_same_style(const struct ui_text_span* a, const struct ui_text_span* b) {
    return a->font == b->font && a->pt_size == b->pt_size && a->weight == b->weight &&
           is_equal_float4(a->color.value, b->color.value) && a->color.color_space == b->color.color_space;
}

static void ui_text_append_span(struct ui_mod_text* mod, uint32_t start, uint32_t end) {
    struct ui_text_span span = {
        .start = start,
        .end = end,
        .font = mod->default_font,
        .pt_size = ui_text_default_pt_size,
        .weight = ui_text_default_weight,
        .color = ui_black,
    };

    for (uint32_t i = 0; i < ui_arr_attribute_ptr_get_count(mod->active); i++) {
        const struct ui_str_attribute* attribute = ui_arr_attribute_ptr_get(mod->active, i);

        switch (attribute->type) {
        case ui_str_attribute_type_none: break;
        case ui_str_attribute_type_color: span.color = attribute->color; break;
        case ui_str_attribute_type_pt_size: span.pt_size = attribute->pt_size; break;
        case ui_str_attribute_type_font: span.font = attribute->font; break;
        case ui_str_attribute_type_weight: span.weight = attribute->weight; break;
        }
    }

    cr_check(span.font != NULL, "text needs a font attribute or a default font");
    cr_check(isfinite(span.pt_size) && span.pt_size >= 0.0 && span.pt_size <= 4096.0, "the text size %g is outside 0 to 4096 points", span.pt_size);
    cr_check(isfinite(span.weight) && span.weight >= 0.0, "the text weight %g is negative", span.weight);

    uint32_t count = ui_arr_text_span_get_count(mod->spans);
    struct ui_text_span* last = count > 0 ? ui_arr_text_span_get_ptr(mod->spans, count - 1) : NULL;

    if (last != NULL && ui_text_span_has_same_style(last, &span)) {
        last->end = end;
    } else {
        ui_arr_text_span_append(mod->spans, span);
    }
}

static void ui_text_resolve_spans(struct ui_mod_text* mod, const struct ui_str* str, uint32_t raw_size) {
    cr_check(str->extra_attributes != NULL || str->extra_attribute_count == 0, "extra attributes need a pointer when their count is not 0");

    ui_arr_attribute_ptr_remove_all(mod->attributes);
    ui_arr_attribute_event_remove_all(mod->events);
    ui_arr_attribute_ptr_remove_all(mod->active);
    ui_arr_text_span_remove_all(mod->spans);

    for (uint32_t i = 0; i < ui_str_attribute_max; i++) {
        ui_text_add_attribute(mod, &str->attributes[i]);
    }
    for (uint32_t i = 0; i < str->extra_attribute_count; i++) {
        ui_text_add_attribute(mod, &str->extra_attributes[i]);
    }

    if (ui_text_attributes_are_uniform(mod, raw_size)) {
        for (uint32_t i = 0; i < ui_arr_attribute_ptr_get_count(mod->attributes); i++) {
            ui_arr_attribute_ptr_append(mod->active, ui_arr_attribute_ptr_get(mod->attributes, i));
        }
        ui_text_append_span(mod, 0, raw_size);
        return;
    }

    ui_text_add_attribute_events(mod, true);
    ui_text_add_attribute_events(mod, false);

    struct ui_attribute_event* events = ui_arr_attribute_event_get_data(mod->events);
    uint32_t event_count = ui_arr_attribute_event_get_count(mod->events);

    if (ui_attribute_events_are_sorted(events, event_count) == false) {
        qsort(events, event_count, sizeof(events[0]), ui_attribute_event_compare);
    }

    uint32_t start = 0;
    uint32_t e = 0;

    do {
        for (; e < event_count && events[e].offset == start; e++) {
            ui_attribute_event_apply(mod->active, &events[e]);
        }

        uint32_t end = e < event_count ? min_uint32(events[e].offset, raw_size) : raw_size;
        ui_text_append_span(mod, start, end);
        start = end;
    } while (start < raw_size);
}

void ui_text_node_init(struct ui* ui, struct ui_node_backing* node, const struct ui_str* str) {
    struct ui_mod_text* mod = ui->mod_text;
    const char* raw = str->raw != NULL ? str->raw : "";
    uint32_t raw_size = (uint32_t)strlen(raw);

    ui_text_resolve_spans(mod, str, raw_size);

    uint32_t span_count = ui_arr_text_span_get_count(mod->spans);
    uint32_t spans_bytes = span_count * (uint32_t)sizeof(struct ui_text_span);
    uint64_t bytes = (uint64_t)spans_bytes + raw_size + 1;

    node->text.block = bytes > sizeof(node->text.small) ? cr_malloc(mod->mem, bytes, 16, cr_loc) : NULL;

    uint8_t* block = node->text.block != NULL ? node->text.block : node->text.small;
    memcpy(block, ui_arr_text_span_get_data(mod->spans), spans_bytes);
    memcpy(block + spans_bytes, raw, raw_size + 1);

    node->text.spans = (const struct ui_text_span*)block;
    node->text.span_count = span_count;
    node->text.raw = (const char*)(block + spans_bytes);
    node->text.raw_size = raw_size;
    node->text.shaped = NULL;
}

static void ui_text_resolve_runs(struct ui_mod_text* mod, const struct ui_node_backing* node) {
    ui_arr_run_remove_all(mod->runs);

    for (uint32_t i = 0; i < node->text.span_count; i++) {
        const struct ui_text_span* span = &node->text.spans[i];
        uint32_t count = ui_arr_run_get_count(mod->runs);
        struct ui_run* last = count > 0 ? ui_arr_run_get_ptr(mod->runs, count - 1) : NULL;

        if (last != NULL && last->font == span->font && last->pt_size == span->pt_size && last->weight == span->weight) {
            last->end = span->end;
            continue;
        }

        struct ui_run run;
        memset(&run, 0, sizeof(run));
        run.pt_size = span->pt_size;
        run.weight = span->weight;
        run.font = span->font;
        run.start = span->start;
        run.end = span->end;
        ui_arr_run_append(mod->runs, run);
    }
}

static struct ui_shaped_key ui_shaped_key_make(struct ui_mod_text* mod, const struct ui_node_backing* node) {
    ui_text_resolve_runs(mod, node);

    struct ui_shaped_key key = {
        .raw = node->text.raw,
        .runs = ui_arr_run_get_data(mod->runs),
        .raw_size = node->text.raw_size,
        .run_count = ui_arr_run_get_count(mod->runs),
    };
    key.hash = cr_hash(key.raw, key.raw_size, cr_hash(key.runs, key.run_count * sizeof(key.runs[0]), 0));

    return key;
}

static void ui_shape_run(struct ui_mod_text* mod, const struct ui_shaped_key* key, uint32_t run_index) {
    const struct ui_run* run = &key->runs[run_index];
    hb_font_t* hb_font = ui_font_hb_font(run->font, ui_font_normalize_weight(run->font, run->weight), ui_font_normalize_opsz(run->font, run->pt_size));
    hb_buffer_t* hb_buf = mod->hb_buf;

    hb_buffer_clear_contents(hb_buf);
    hb_buffer_add_utf8(hb_buf, key->raw, (int)key->raw_size, run->start, (int)(run->end - run->start));
    hb_buffer_set_direction(hb_buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(hb_buf, mod->hb_lang);

    hb_shape(hb_font, hb_buf, NULL, 0);

    uint32_t count;
    const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(hb_buf, &count);
    const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(hb_buf, &count);

    for (uint32_t i = 0; i < count; i++) {
        ui_arr_shaped_glyph_append(mod->glyphs, (struct ui_shaped_glyph) {
            .advance = key->raw[infos[i].cluster] == '\n' ? 0 : positions[i].x_advance,
            .offset = infos[i].cluster,
            .glyph_id = (uint16_t)infos[i].codepoint,
            .run = (uint16_t)run_index,
        });
    }
}

static struct ui_shaped* ui_shaped_create(struct ui_mod_text* mod, const struct ui_shaped_key* key) {
    assert(key->run_count <= uint16_max);

    ui_arr_shaped_glyph_remove_all(mod->glyphs);
    for (uint32_t i = 0; i < key->run_count; i++) {
        ui_shape_run(mod, key, i);
    }

    uint32_t glyph_count = ui_arr_shaped_glyph_get_count(mod->glyphs);
    uint64_t header_size = align_up_uint64(sizeof(struct ui_shaped), 16);
    uint64_t bytes = header_size + key->run_count * sizeof(struct ui_run) + glyph_count * sizeof(struct ui_shaped_glyph) + key->raw_size + 1;

    struct ui_shaped* shaped = cr_malloc(mod->mem, bytes, 16, cr_loc);
    struct ui_run* runs = (void*)((uint8_t*)shaped + header_size);
    struct ui_shaped_glyph* glyphs = (void*)(runs + key->run_count);
    char* raw = (char*)(glyphs + glyph_count);

    memcpy(runs, key->runs, key->run_count * sizeof(runs[0]));
    memcpy(glyphs, ui_arr_shaped_glyph_get_data(mod->glyphs), glyph_count * sizeof(glyphs[0]));
    memcpy(raw, key->raw, key->raw_size + 1);

    *shaped = (struct ui_shaped) {
        .key = *key,
        .rc = 1,
        .glyph_count = glyph_count,
        .bytes = bytes,
        .glyphs = glyphs,
    };
    shaped->key.raw = raw;
    shaped->key.runs = runs;

    return shaped;
}

static void ui_shaped_free_lines(struct ui_mod_text* mod, struct ui_shaped* shaped, struct ui_shaped_lines** lines) {
    if (*lines != NULL) {
        shaped->bytes -= sizeof(**lines) + (*lines)->count * sizeof((*lines)->lines[0]);
        cr_free(mod->mem, *lines, cr_loc);
        *lines = NULL;
    }
}

static void ui_shaped_free(struct ui_mod_text* mod, struct ui_shaped* shaped) {
    ui_shaped_free_lines(mod, shaped, &shaped->natural);
    ui_shaped_free_lines(mod, shaped, &shaped->constrained);
    cr_free(mod->mem, shaped, cr_loc);
}

static void ui_shaped_link_unused(struct ui_mod_text* mod, struct ui_shaped* shaped) {
    shaped->prev = NULL;
    shaped->next = mod->unused_head;
    if (mod->unused_head != NULL) {
        mod->unused_head->prev = shaped;
    } else {
        mod->unused_tail = shaped;
    }
    mod->unused_head = shaped;
    mod->unused_bytes += shaped->bytes;
}

static void ui_shaped_unlink_unused(struct ui_mod_text* mod, struct ui_shaped* shaped) {
    if (shaped->prev != NULL) {
        shaped->prev->next = shaped->next;
    } else {
        mod->unused_head = shaped->next;
    }
    if (shaped->next != NULL) {
        shaped->next->prev = shaped->prev;
    } else {
        mod->unused_tail = shaped->prev;
    }
    mod->unused_bytes -= shaped->bytes;
}

static void ui_shaped_evict(struct ui_mod_text* mod, struct ui_shaped* shaped) {
    ui_shaped_unlink_unused(mod, shaped);
    ui_dict_shaped_remove(mod->cache, &shaped->key);
    ui_shaped_free(mod, shaped);
}

static void ui_shaped_release(struct ui_mod_text* mod, struct ui_shaped* shaped) {
    assert(shaped->rc > 0);
    if (--shaped->rc > 0) {
        return;
    }

    ui_shaped_link_unused(mod, shaped);

    while (mod->unused_bytes > ui_text_unused_bytes_max) {
        ui_shaped_evict(mod, mod->unused_tail);
    }
}

static struct ui_shaped* ui_text_shaped(struct ui* ui, struct ui_node_backing* node) {
    if (node->text.shaped != NULL) {
        return node->text.shaped;
    }

    struct ui_mod_text* mod = ui->mod_text;
    struct ui_shaped_key key = ui_shaped_key_make(mod, node);
    struct ui_shaped* shaped;

    if (ui_dict_shaped_get(mod->cache, &key, &shaped)) {
        if (shaped->rc++ == 0) {
            ui_shaped_unlink_unused(mod, shaped);
        }
    } else {
        shaped = ui_shaped_create(mod, &key);
        ui_dict_shaped_set(mod->cache, &shaped->key, shaped);
    }

    node->text.shaped = shaped;

    return shaped;
}

void ui_text_node_deinit(struct ui* ui, struct ui_node_backing* node) {
    if (node->text.shaped != NULL) {
        ui_shaped_release(ui->mod_text, node->text.shaped);
    }
    if (node->text.block != NULL) {
        cr_free(ui->mem, node->text.block, cr_loc);
    }
}

static bool ui_shaped_uses_font(const struct ui_shaped* shaped, const struct ui_font* font) {
    for (uint32_t i = 0; i < shaped->key.run_count; i++) {
        if (shaped->key.runs[i].font == font) {
            return true;
        }
    }
    return false;
}

static void ui_mod_text_forget_font(struct ui* ui, struct ui_font* font) {
    struct ui_mod_text* mod = ui->mod_text;

    for (uint32_t i = 0; i < cr_count(ui->node_pool.items); i++) {
        const struct ui_node_backing* node = &ui->node_pool.items[i];

        if (ui_node_is_null(node->handle) || node->type != ui_content_type_text) {
            continue;
        }
        for (uint32_t s = 0; s < node->text.span_count; s++) {
            cr_check(node->text.spans[s].font != font, "a font cannot be destroyed while text still uses it");
        }
    }

    struct ui_shaped* next;
    for (struct ui_shaped* shaped = mod->unused_head; shaped != NULL; shaped = next) {
        next = shaped->next;
        if (ui_shaped_uses_font(shaped, font)) {
            ui_shaped_evict(mod, shaped);
        }
    }
}

static double ui_run_scale(const struct ui_run* run) {
    return run->pt_size / (double)(run->font->upem << 6);
}

static double ui_font_units_to_pt(const struct ui_font* font, int32_t units, double pt_size) {
    return (double)units * pt_size / (double)font->upem;
}

static struct ui_line_metrics ui_run_metrics(const struct ui_run* run) {
    double ascender_pt = ui_font_units_to_pt(run->font, run->font->ascender, run->pt_size);
    double descender_pt = ui_font_units_to_pt(run->font, run->font->descender, run->pt_size);
    double height_pt = ui_font_units_to_pt(run->font, run->font->height, run->pt_size);

    return (struct ui_line_metrics) {
        .ascender_pt = ascender_pt,
        .descender_pt = descender_pt,
        .gap_pt = height_pt - (ascender_pt - descender_pt),
    };
}

static struct ui_line_metrics ui_line_metrics_merge(struct ui_line_metrics a, struct ui_line_metrics b) {
    return (struct ui_line_metrics) {
        .ascender_pt = max_double(a.ascender_pt, b.ascender_pt),
        .descender_pt = min_double(a.descender_pt, b.descender_pt),
        .gap_pt = max_double(a.gap_pt, b.gap_pt),
    };
}

static struct ui_line_metrics ui_shaped_line_metrics(const struct ui_shaped* shaped, uint32_t glyph_start, uint32_t glyph_end) {
    uint32_t first = glyph_start < shaped->glyph_count ? shaped->glyphs[glyph_start].run : shaped->key.run_count - 1;
    uint32_t last = glyph_end > glyph_start ? shaped->glyphs[glyph_end - 1].run : first;
    struct ui_line_metrics metrics = ui_run_metrics(&shaped->key.runs[first]);

    for (uint32_t r = first + 1; r <= last; r++) {
        metrics = ui_line_metrics_merge(metrics, ui_run_metrics(&shaped->key.runs[r]));
    }

    return metrics;
}

static void ui_shaped_lines_append(struct ui_mod_text* mod, const struct ui_shaped* shaped, uint32_t glyph_start, uint32_t glyph_end, double width_pt) {
    uint32_t count = ui_arr_shaped_line_get_count(mod->lines);
    const struct ui_shaped_line* prev = count > 0 ? ui_arr_shaped_line_get_ptr(mod->lines, count - 1) : NULL;
    struct ui_line_metrics metrics = ui_shaped_line_metrics(shaped, glyph_start, glyph_end);

    ui_arr_shaped_line_append(mod->lines, (struct ui_shaped_line) {
        .glyph_start = glyph_start,
        .glyph_end = glyph_end,
        .y_pt = prev != NULL ? prev->y_pt + prev->height_pt : 0.0,
        .ascender_pt = metrics.ascender_pt,
        .height_pt = (metrics.ascender_pt - metrics.descender_pt) + metrics.gap_pt,
        .width_pt = width_pt,
    });
}

static struct ui_shaped_lines* ui_shaped_lines_build(struct ui_mod_text* mod, struct ui_shaped* shaped, int32_t wrap_width_266) {
    const struct ui_shaped_glyph* glyphs = shaped->glyphs;
    const char* raw = shaped->key.raw;
    double wrap_width_pt = (double)wrap_width_266 / 64.0 + ui_text_pen_epsilon;
    uint32_t line_start = 0;
    uint32_t word_start = 0;
    uint32_t run_index = uint32_max;
    double scale = 0.0;
    double x_pt = 0.0;
    double word_x_pt = 0.0;
    double width_pt = 0.0;
    bool after_space = true;

    ui_arr_shaped_line_remove_all(mod->lines);

    for (uint32_t g = 0; g <= shaped->glyph_count; g++) {
        bool is_end = g == shaped->glyph_count;
        char c = is_end ? '\n' : raw[glyphs[g].offset];
        bool is_newline = c == '\n';
        bool is_space = is_newline || c == ' ';
        double advance_pt = 0.0;

        if (is_end == false) {
            if (glyphs[g].run != run_index) {
                run_index = glyphs[g].run;
                scale = ui_run_scale(&shaped->key.runs[run_index]);
            }
            advance_pt = (double)glyphs[g].advance * scale;
        }
        if (after_space && is_space == false) {
            word_start = g;
            word_x_pt = x_pt;
        }

        if (is_newline) {
            ui_shaped_lines_append(mod, shaped, line_start, is_end ? g : g + 1, x_pt);
            width_pt = max_double(width_pt, x_pt);
            line_start = g + 1;
            x_pt = 0.0;
            word_x_pt = 0.0;
        } else if (is_space == false && x_pt + advance_pt > wrap_width_pt && g > line_start) {
            if (word_start > line_start) {
                ui_shaped_lines_append(mod, shaped, line_start, word_start, word_x_pt);
                width_pt = max_double(width_pt, word_x_pt);
                line_start = word_start;
                x_pt -= word_x_pt;
            } else {
                ui_shaped_lines_append(mod, shaped, line_start, g, x_pt);
                width_pt = max_double(width_pt, x_pt);
                line_start = g;
                word_start = g;
                x_pt = 0.0;
            }
            word_x_pt = 0.0;
        }

        x_pt += advance_pt;
        after_space = is_space;
    }

    uint32_t count = ui_arr_shaped_line_get_count(mod->lines);
    const struct ui_shaped_line* last = ui_arr_shaped_line_get_ptr(mod->lines, count - 1);
    uint64_t bytes = sizeof(struct ui_shaped_lines) + count * sizeof(struct ui_shaped_line);
    struct ui_shaped_lines* lines = cr_malloc(mod->mem, bytes, 0, cr_loc);

    lines->wrap_width_266 = wrap_width_266;
    lines->width_266 = (int32_t)ceil(width_pt * 64.0 - ui_text_pen_epsilon);
    lines->height_266 = (int32_t)ceil((last->y_pt + last->height_pt) * 64.0 - ui_text_pen_epsilon);
    lines->count = count;
    memcpy(lines->lines, ui_arr_shaped_line_get_data(mod->lines), count * sizeof(struct ui_shaped_line));
    shaped->bytes += bytes;

    return lines;
}

static const struct ui_shaped_lines* ui_shaped_lines(struct ui_mod_text* mod, struct ui_shaped* shaped, int32_t wrap_width_266) {
    if (shaped->natural == NULL) {
        shaped->natural = ui_shaped_lines_build(mod, shaped, int32_max);
    }
    if (wrap_width_266 >= shaped->natural->width_266) {
        return shaped->natural;
    }
    if (shaped->constrained == NULL || shaped->constrained->wrap_width_266 != wrap_width_266) {
        ui_shaped_free_lines(mod, shaped, &shaped->constrained);
        shaped->constrained = ui_shaped_lines_build(mod, shaped, wrap_width_266);
    }
    return shaped->constrained;
}

struct cr_rect_int32 ui_text_ink_rect_px(const struct ui_node_backing* node, struct cr_rect_int32 rect_px, double to_px_from_pt) {
    double side_px = 0.0;
    double above_px = 0.0;
    double below_px = 0.0;

    for (uint32_t i = 0; i < node->text.span_count; i++) {
        const struct ui_text_span* span = &node->text.spans[i];
        double size_px = span->pt_size * to_px_from_pt;

        side_px = max_double(side_px, size_px * ui_text_overhang_em);
        above_px = max_double(above_px, size_px * (double)span->font->ink_above_ascender / (double)span->font->upem);
        below_px = max_double(below_px, size_px * (double)span->font->ink_below_descender / (double)span->font->upem);
    }

    int2 start = rect_px.origin - make_int2((int32_t)ceil(side_px), (int32_t)ceil(above_px));
    int2 end = rect_px.origin + rect_px.size + make_int2((int32_t)ceil(side_px), (int32_t)ceil(below_px));

    return (struct cr_rect_int32) { .origin = start, .size = end - start };
}

int2 ui_text_measure(struct ui* ui, struct ui_node_backing* node, int32_t width_266) {
    struct ui_shaped* shaped = ui_text_shaped(ui, node);

    if (shaped->glyph_count == 0) {
        return make_int2(0, 0);
    }

    const struct ui_shaped_lines* lines = ui_shaped_lines(ui->mod_text, shaped, width_266);

    return make_int2(lines->width_266, lines->height_266);
}

static bool ui_text_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static struct ui_text_style ui_text_style_make(const struct ui_text_span* span, double to_px_from_pt) {
    struct ui_font* font = span->font;
    uint32_t size_266_px = ui_glyph_size_266_snap((uint32_t)lround(span->pt_size * to_px_from_pt * 64.0));

    return (struct ui_text_style) {
        .key = ui_glyph_key_make(font->id, size_266_px, ui_font_normalize_weight(font, span->weight), ui_font_normalize_opsz(font, (double)size_266_px / (64.0 * to_px_from_pt))),
        .phase_count = ui_glyph_phase_count(size_266_px),
        .scale = span->pt_size / (double)(font->upem << 6),
        .color = ui_to_compositing_from_color(span->color),
    };
}

static bool ui_line_is_clipped(double top_px, double height_px, struct cr_rect_int32 clip_px) {
    double slack_px = height_px;
    double bottom_px = top_px + height_px;

    return bottom_px + slack_px <= (double)clip_px.origin.y || top_px - slack_px >= (double)(clip_px.origin.y + clip_px.size.y);
}

void ui_text_draw(struct ui* ui, struct ui_node_backing* node, double to_px_from_pt, int2 origin_266_px, int32_t width_266, struct cr_rect_int32 clip_px, uint64_t serial) {
    struct ui_shaped* shaped = ui_text_shaped(ui, node);
    const struct ui_shaped_lines* lines = ui_shaped_lines(ui->mod_text, shaped, width_266);
    const struct ui_text_span* span = node->text.spans;
    struct ui_text_style style = ui_text_style_make(span, to_px_from_pt);
    double origin_x_px = (double)origin_266_px.x / 64.0 + ui_text_pen_epsilon;
    double origin_y_px = (double)origin_266_px.y / 64.0;

    for (uint32_t l = 0; l < lines->count; l++) {
        const struct ui_shaped_line* line = &lines->lines[l];

        if (ui_line_is_clipped(origin_y_px + line->y_pt * to_px_from_pt, line->height_pt * to_px_from_pt, clip_px)) {
            continue;
        }

        int32_t baseline_px = (int32_t)ceil(origin_y_px + (line->y_pt + line->ascender_pt) * to_px_from_pt - ui_text_pen_epsilon);
        double x_pt = 0.0;

        for (uint32_t g = line->glyph_start; g < line->glyph_end; g++) {
            struct ui_shaped_glyph* glyph = &shaped->glyphs[g];

            while (glyph->offset >= span->end) {
                span++;
                style = ui_text_style_make(span, to_px_from_pt);
            }

            if (style.key.size_266_px > 0 && glyph->glyph_id != 0 && ui_text_is_space(shaped->key.raw[glyph->offset]) == false) {
                double pen_x_px = origin_x_px + x_pt * to_px_from_pt;
                int2 pen_px = make_int2((int32_t)floor(pen_x_px), baseline_px);
                struct ui_glyph_key key = style.key;

                key.glyph_id = glyph->glyph_id;
                key.phase_x_64 = ui_glyph_phase_x_64(max_double(pen_x_px - (double)pen_px.x, 0.0), style.phase_count);

                const struct ui_glyph* atlas_glyph = ui_atlas_glyph(ui, span->font, key, serial, &glyph->slot_hint);

                if (atlas_glyph->bind_group != NULL) {
                    ui_render_add_glyph(ui, atlas_glyph, pen_px, style.color);
                }
            }

            x_pt += (double)glyph->advance * style.scale;
        }
    }
}

static struct ui_node_backing* ui_text_node(struct ui* ui, struct ui_node text) {
    struct ui_node_backing* node = ui_node_check(ui, text, "the text node");
    cr_check(node->type == ui_content_type_text, "the node is not a text node");

    return node;
}

static const struct ui_shaped_lines* ui_text_node_lines(struct ui* ui, struct ui_node_backing* node) {
    return ui_shaped_lines(ui->mod_text, ui_text_shaped(ui, node), ui_fx_to_text_266(node->p2.width, ui->to_px_from_pt));
}

static double ui_glyph_advance_pt(const struct ui_shaped* shaped, const struct ui_shaped_glyph* glyph) {
    return (double)glyph->advance * ui_run_scale(&shaped->key.runs[glyph->run]);
}

static uint32_t ui_text_line_start(const struct ui_shaped* shaped, const struct ui_shaped_line* line) {
    return line->glyph_start < shaped->glyph_count ? shaped->glyphs[line->glyph_start].offset : shaped->key.raw_size;
}

static struct ui_text_line ui_text_line_make(const struct ui_shaped* shaped, const struct ui_shaped_lines* lines, uint32_t index) {
    cr_check(index < lines->count, "the line index %u is out of range for %u lines", index, lines->count);

    const struct ui_shaped_line* line = &lines->lines[index];
    uint32_t end = index + 1 < lines->count ? ui_text_line_start(shaped, line + 1) : shaped->key.raw_size;
    uint32_t visual_end = end;

    if (line->glyph_end > line->glyph_start) {
        uint32_t last_offset = shaped->glyphs[line->glyph_end - 1].offset;

        if (shaped->key.raw[last_offset] == '\n') {
            visual_end = last_offset;
        }
    }

    return (struct ui_text_line) {
        .start = ui_text_line_start(shaped, line),
        .end = end,
        .visual_end = visual_end,
        .y = line->y_pt,
        .height = line->height_pt,
        .width = line->width_pt,
    };
}

uint32_t ui_text_get_line_count(struct ui_node text) {
    struct ui* ui = ui_ctx();

    return ui_text_node_lines(ui, ui_text_node(ui, text))->count;
}

struct ui_text_line ui_text_get_line(struct ui_node text, uint32_t line_index) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_text_node(ui, text);
    const struct ui_shaped_lines* lines = ui_text_node_lines(ui, node);

    return ui_text_line_make(node->text.shaped, lines, line_index);
}

double ui_text_get_x(struct ui_node text, uint32_t line_index, uint32_t offset) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_text_node(ui, text);
    const struct ui_shaped_lines* lines = ui_text_node_lines(ui, node);
    const struct ui_shaped* shaped = node->text.shaped;

    cr_check(line_index < lines->count, "the line index %u is out of range for %u lines", line_index, lines->count);
    const struct ui_shaped_line* line = &lines->lines[line_index];

    double x = 0.0;
    for (uint32_t g = line->glyph_start; g < line->glyph_end && shaped->glyphs[g].offset < offset; g++) {
        x += ui_glyph_advance_pt(shaped, &shaped->glyphs[g]);
    }
    return x;
}

uint32_t ui_text_get_offset(struct ui_node text, uint32_t line_index, double x_pt) {
    struct ui* ui = ui_ctx();

    struct ui_node_backing* node = ui_text_node(ui, text);
    const struct ui_shaped_lines* lines = ui_text_node_lines(ui, node);
    const struct ui_shaped* shaped = node->text.shaped;
    struct ui_text_line text_line = ui_text_line_make(shaped, lines, line_index);
    const struct ui_shaped_line* line = &lines->lines[line_index];

    double x = 0.0;
    uint32_t g = line->glyph_start;

    while (g < line->glyph_end) {
        uint32_t cluster = shaped->glyphs[g].offset;
        double cluster_advance = 0.0;

        for (; g < line->glyph_end && shaped->glyphs[g].offset == cluster; g++) {
            cluster_advance += ui_glyph_advance_pt(shaped, &shaped->glyphs[g]);
        }

        if (cluster < text_line.visual_end && x_pt < x + cluster_advance * 0.5) {
            return cluster;
        }

        x += cluster_advance;
    }

    return text_line.visual_end;
}

uint32_t ui_text_prev_cluster(struct ui_node text, uint32_t offset) {
    struct ui* ui = ui_ctx();

    const struct ui_shaped* shaped = ui_text_shaped(ui, ui_text_node(ui, text));
    uint32_t prev = 0;

    for (uint32_t g = 0; g < shaped->glyph_count && shaped->glyphs[g].offset < offset; g++) {
        prev = shaped->glyphs[g].offset;
    }

    return prev;
}

uint32_t ui_text_next_cluster(struct ui_node text, uint32_t offset) {
    struct ui* ui = ui_ctx();

    const struct ui_shaped* shaped = ui_text_shaped(ui, ui_text_node(ui, text));

    for (uint32_t g = 0; g < shaped->glyph_count; g++) {
        if (shaped->glyphs[g].offset > offset) {
            return shaped->glyphs[g].offset;
        }
    }

    return shaped->key.raw_size;
}

uint64_t ui_text_unused_bytes_(void) {
    return ui_ctx()->mod_text->unused_bytes;
}
