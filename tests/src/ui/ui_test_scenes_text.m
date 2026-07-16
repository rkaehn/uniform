#include "ui_tests.h"
#include "ui_test_glyphs.h"
#include "ui_core.h"
#include <stdio.h>

#if cr_ios
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#endif

#define ui_test_inter_opsz_min 14.0
#define ui_test_inter_opsz_max 32.0

struct ui_test_scene_text_item {
    const char* raw;
    double x;
    double y;
    double pt_size;
    struct ui_color color;
    double weight;
    bool inter;
};

static const struct ui_test_scene_text_item ui_test_scene_text_items_[] = {
    { "compositor", 32.0, 32.0, 17.0, ui_label, 0.0, false },
    { "glyph atlas", 32.0, 80.0, 34.0, ui_system_blue, 0.0, false },
    { "scissor", 32.0, 160.0, 13.0, ui_system_red, 0.0, false },
    { "blend pass", 32.0, 200.0, 22.0, ui_secondary_label, 0.0, false },
    { "vertex", 32.0, 260.0, 48.0, ui_system_purple, 0.0, false },
    { "texture quad texture quad texture quad", 32.0, 350.0, 17.0, ui_label, 0.0, false },
    { "tertiary ink", 32.0, 390.0, 22.0, ui_tertiary_label, 0.0, false },
};

static const struct ui_test_scene_text_item ui_test_scene_text_weights_items_[] = {
    { "thin quartz glyphs", 32.0, 32.0, 17.0, ui_label, 100.0, false },
    { "light quartz glyphs", 32.0, 64.0, 17.0, ui_label, 300.0, false },
    { "regular quartz glyphs", 32.0, 96.0, 17.0, ui_label, 400.0, false },
    { "medium quartz glyphs", 32.0, 128.0, 17.0, ui_label, 500.0, false },
    { "bold quartz glyphs", 32.0, 160.0, 17.0, ui_label, 700.0, false },
    { "extra bold", 32.0, 192.0, 13.0, ui_system_brown, 800.0, false },
    { "thin interface", 32.0, 240.0, 30.0, ui_system_indigo, 100.0, true },
    { "light interface", 32.0, 284.0, 30.0, ui_system_teal, 300.0, true },
    { "semibold interface", 32.0, 328.0, 30.0, ui_label, 600.0, true },
    { "extra black interface", 32.0, 372.0, 30.0, ui_label, 900.0, true },
    { "medium small", 32.0, 428.0, 11.0, ui_secondary_label, 500.0, true },
};

static const struct ui_test_scene_text_item ui_test_scene_text_inter_items_[] = {
    { "optical size small", 32.0, 32.0, 9.0, ui_label, 0.0, true },
    { "the quick brown fox jumps over the lazy dog", 32.0, 52.0, 13.0, ui_label, 0.0, true },
    { "system interface text", 32.0, 80.0, 17.0, ui_system_blue, 0.0, true },
    { "tracking and advances", 32.5, 120.0, 17.0, ui_system_red, 0.0, true },
    { "display range", 32.0, 160.0, 26.0, ui_secondary_label, 0.0, true },
    { "headline", 32.0, 210.0, 40.0, ui_system_purple, 0.0, true },
    { "mixed mono and interface", 32.0, 280.0, 17.0, ui_label, 0.0, false },
    { "wide walrus warble", 32.0, 320.0, 13.0, ui_label, 0.0, true },
    { "app scale fourteen forty five", 32.0, 350.0, 14.45, ui_label, 0.0, true },
    { "app scale eleven oh five", 32.0, 384.0, 11.05, ui_system_brown, 0.0, true },
    { "beyond optical range", 32.0, 410.0, 44.0, ui_quaternary_label, 0.0, true },
};

static const struct ui_test_scene_text_item ui_test_scene_text_fractional_items_[] = {
    { "quarter offset", 32.25, 32.0, 17.0, ui_label, 0.0, false },
    { "half offset", 32.5, 64.0, 17.0, ui_label, 0.0, false },
    { "fractional baseline", 32.0, 96.25, 17.0, ui_label, 0.0, false },
    { "fractional size", 32.0, 130.0, 13.5, ui_system_brown, 0.0, false },
    { "sixteen and a half", 32.0, 160.0, 16.5, ui_system_blue, 0.0, false },
    { "twenty and a quarter", 32.0, 200.0, 20.25, ui_label, 0.0, false },
    { "both fractional", 32.75, 240.5, 13.5, ui_label, 0.0, false },
    { "inter quarter offset", 32.25, 280.0, 17.0, ui_label, 0.0, true },
    { "inter fractional size", 32.0, 312.5, 15.5, ui_secondary_label, 0.0, true },
};

#define ui_test_atlas_margin 32.0
#define ui_test_atlas_cols 20
#define ui_test_atlas_pitch 48.0
#define ui_test_atlas_pt 28.0
#define ui_test_atlas_small_margin 16.0
#define ui_test_atlas_small_cols 24
#define ui_test_atlas_small_pitch 20.0
#define ui_test_atlas_small_pt 12.0

#define ui_test_atlas_glyph_max 256

struct ui_test_atlas_layout {
    const uint32_t* codepoints;
    uint32_t count;
    double margin;
    uint32_t cols;
    double pitch;
    double pt_size;
    bool inter;
};

static struct ui_test_atlas_layout ui_test_atlas_layout(uint32_t index) {
    switch (index) {
    case 0:
        return (struct ui_test_atlas_layout) {
            .codepoints = ui_test_glyphs_ui_, .count = cr_count(ui_test_glyphs_ui_),
            .margin = ui_test_atlas_margin, .cols = ui_test_atlas_cols, .pitch = ui_test_atlas_pitch,
            .pt_size = ui_test_atlas_pt, .inter = true,
        };
    case 1:
        return (struct ui_test_atlas_layout) {
            .codepoints = ui_test_glyphs_mono_, .count = cr_count(ui_test_glyphs_mono_),
            .margin = ui_test_atlas_margin, .cols = ui_test_atlas_cols, .pitch = ui_test_atlas_pitch,
            .pt_size = ui_test_atlas_pt, .inter = false,
        };
    default:
        return (struct ui_test_atlas_layout) {
            .codepoints = ui_test_glyphs_ui_, .count = cr_count(ui_test_glyphs_ui_),
            .margin = ui_test_atlas_small_margin, .cols = ui_test_atlas_small_cols, .pitch = ui_test_atlas_small_pitch,
            .pt_size = ui_test_atlas_small_pt, .inter = true,
        };
    }
}

static uint32_t ui_test_atlas_utf8(uint32_t cp, char* out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = 0;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        out[2] = 0;
        return 2;
    }
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[2] = (char)(0x80 | (cp & 0x3f));
    out[3] = 0;
    return 3;
}

static uint32_t ui_test_atlas_items(uint32_t layout_index, struct ui_test_scene_text_item* out, char* utf8, uint32_t utf8_stride) {
    struct ui_test_atlas_layout layout = ui_test_atlas_layout(layout_index);
    assert(layout.count <= ui_test_atlas_glyph_max);

    for (uint32_t i = 0; i < layout.count; i++) {
        char* raw = utf8 + (uint64_t)i * utf8_stride;
        ui_test_atlas_utf8(layout.codepoints[i], raw);
        out[i] = (struct ui_test_scene_text_item) {
            .raw = raw,
            .x = layout.margin + (double)(i % layout.cols) * layout.pitch,
            .y = layout.margin + (double)(i / layout.cols) * layout.pitch,
            .pt_size = layout.pt_size,
            .color = ui_label,
            .inter = layout.inter,
        };
    }
    return layout.count;
}

struct ui_test_text_state {
    struct ui_test_scene_state base;
    struct ui_font* inter;
    const struct ui_test_scene_text_item* items;
    uint32_t item_count;
    const char* text;
};

struct ui_test_text_desc {
    const struct ui_test_scene_text_item* items;
    uint32_t item_count;
};

static void ui_test_text_state_font(struct ui_test_text_state* text) {
    for (uint32_t i = 0; i < text->item_count; i++) {
        if (text->items[i].inter) {
            uint64_t size;
            const uint8_t* data = ui_test_scenes_font_(text->base.scenes, true, &size);
            text->inter = ui_font_create(data, size);
            return;
        }
    }
}

static void ui_test_text_state_destroy(struct ui_test_scene_state* state) {
    struct ui_test_text_state* text = (struct ui_test_text_state*)state;
    if (text->inter != NULL) {
        ui_font_destroy(text->inter);
    }
    cr_free(state->mem, state, cr_loc);
}

static struct ui_node ui_test_text_state_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_test_text_state* text = (struct ui_test_text_state*)state;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_system_background),
        .clip = true,
    });

    for (uint32_t i = 0; i < text->item_count; i++) {
        const struct ui_test_scene_text_item* item = &text->items[i];

        ui_text(&(struct ui_text) {
            .parent = root,
            .pos = ui_pos_xy(item->x, item->y),
            .str = {
                .raw = item->raw,
                .attributes = {
                    { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_color, .color = item->color },
                    { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_pt_size, .pt_size = item->pt_size },
                    item->weight != 0.0
                        ? (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_weight, .weight = item->weight }
                        : (struct ui_str_attribute) { 0 },
                    item->inter
                        ? (struct ui_str_attribute) { .start = 0, .end = uint32_max, .type = ui_str_attribute_type_font, .font = text->inter }
                        : (struct ui_str_attribute) { 0 },
                },
            },
        });
    }

    return root;
}

#if cr_ios
static CGFontRef ui_test_scene_text_cg_font(const struct ui_test_scenes* scenes, bool inter) {
    uint64_t size;
    const uint8_t* data = ui_test_scenes_font_(scenes, inter, &size);

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, data, size, NULL);
    CGFontRef cg_font = CGFontCreateWithDataProvider(provider);
    CGDataProviderRelease(provider);

    return cg_font;
}

CTFontRef ui_test_scene_text_ct_font_(CGFontRef cg_font, double pt_size, double weight, bool inter) {
    double resolved_weight = weight != 0.0 ? weight : 400.0;

    NSMutableDictionary* variation = [NSMutableDictionary dictionary];
    variation[@(0x77676874u)] = @(resolved_weight);
    if (inter) {
        variation[@(0x6f70737au)] = @(clamp_double(pt_size, ui_test_inter_opsz_min, ui_test_inter_opsz_max));
    }

    NSDictionary* descriptor_attributes = @{ (id)kCTFontVariationAttribute: variation };
    CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes((CFDictionaryRef)descriptor_attributes);

    CTFontRef base = CTFontCreateWithGraphicsFont(cg_font, pt_size, NULL, NULL);
    CTFontRef font = CTFontCreateCopyWithAttributes(base, pt_size, NULL, descriptor);

    CFRelease(base);
    CFRelease(descriptor);

    return font;
}

static void ui_test_scene_text_items_pixels(const struct ui_test_scenes* scenes, const struct ui_test_scene_text_item* items, uint32_t item_count,
                                            uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_system_background);

    CGFontRef cg_font_mono = ui_test_scene_text_cg_font(scenes, false);
    CGFontRef cg_font_inter = ui_test_scene_text_cg_font(scenes, true);

    CGContextScaleCTM(ctx, to_px_from_pt, to_px_from_pt);
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));

    for (uint32_t i = 0; i < item_count; i++) {
        const struct ui_test_scene_text_item* item = &items[i];

        CTFontRef font = ui_test_scene_text_ct_font_(item->inter ? cg_font_inter : cg_font_mono, item->pt_size, item->weight, item->inter);

        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGFloat components[4] = { item->color.value.x, item->color.value.y, item->color.value.z, item->color.value.w };
        CGColorRef color = CGColorCreate(space, components);
        CGColorSpaceRelease(space);

        NSDictionary* attributes = @{
            (id)kCTFontAttributeName: (__bridge id)font,
            (id)kCTForegroundColorAttributeName: (__bridge id)color,
        };
        NSAttributedString* str = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:item->raw] attributes:attributes];
        CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)str);

        double x = round(item->x * to_px_from_pt) / to_px_from_pt;
        double y = round(item->y * to_px_from_pt) / to_px_from_pt;
        double baseline = y + CTFontGetAscent(font);
        CGContextSetTextPosition(ctx, x, baseline);
        CTLineDraw(line, ctx);

        CFRelease(line);
        [str release];
        CGColorRelease(color);
        CFRelease(font);
    }

    CGFontRelease(cg_font_inter);
    CGFontRelease(cg_font_mono);

    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}

static void ui_test_text_state_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    struct ui_test_text_state* text = (struct ui_test_text_state*)state;
    ui_test_scene_text_items_pixels(state->scenes, text->items, text->item_count, width, height, to_px_from_pt, out_rgba, bytes_per_row);
}
#endif

static struct ui_test_text_state* ui_test_text_state_alloc(struct ui_test_scenes* scenes, struct cr_mem* mem, uint64_t extra, void** out_extra) {
    uint64_t state_size = (sizeof(struct ui_test_text_state) + 15ull) & ~15ull;
    struct ui_test_text_state* text = cr_malloc(mem, state_size + extra, 16, cr_loc);
    memset(text, 0, (size_t)(state_size + extra));
    text->base = (struct ui_test_scene_state) {
        .scenes = scenes,
        .mem = mem,
        .destroy = ui_test_text_state_destroy,
    };
    if (out_extra != NULL) {
        *out_extra = (uint8_t*)text + state_size;
    }
    return text;
}

static struct ui_test_scene_state* ui_test_text_items_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    const struct ui_test_text_desc* desc = ctx;
    struct ui_test_text_state* text = ui_test_text_state_alloc(scenes, mem, 0, NULL);
    text->items = desc->items;
    text->item_count = desc->item_count;
    ui_test_text_state_font(text);
    return &text->base;
}

#define ui_test_text_scene(items_table) \
    .create = ui_test_text_items_create, \
    .ctx = &(const struct ui_test_text_desc) { .items = items_table, .item_count = cr_count(items_table) }, \
    .build = ui_test_text_state_build, \
    .pixels = ui_test_scene_pixels(ui_test_text_state_pixels)

static struct ui_test_scene_state* ui_test_text_atlas_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    uint32_t layout_index = *(const uint32_t*)ctx;

    void* extra;
    struct ui_test_text_state* text = ui_test_text_state_alloc(scenes, mem, ui_test_atlas_glyph_max * (sizeof(struct ui_test_scene_text_item) + 4), &extra);
    struct ui_test_scene_text_item* items = extra;
    char* utf8 = (char*)(items + ui_test_atlas_glyph_max);

    text->items = items;
    text->item_count = ui_test_atlas_items(layout_index, items, utf8, 4);
    ui_test_text_state_font(text);
    return &text->base;
}

#define ui_test_text_atlas_scene(layout_index) \
    .create = ui_test_text_atlas_create, \
    .ctx = &(const uint32_t) { layout_index }, \
    .build = ui_test_text_state_build, \
    .pixels = ui_test_scene_pixels(ui_test_text_state_pixels)

static const char* ui_test_scene_text_lines_raw_13_ = "pack my box with\nfive dozen liquor jugs\nhow vexingly quick\ndaft zebras jump";
static const char* ui_test_scene_text_lines_raw_22_ = "sphinx of black quartz\njudge my vow";

static struct ui_node ui_test_scene_text_lines_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_system_background),
        .clip = true,
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 32.0),
        .str = ui_str(ui_test_scene_text_lines_raw_13_, ui_label, 13.0),
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(32.0, 140.0),
        .str = ui_str(ui_test_scene_text_lines_raw_22_, ui_label, 22.0),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_text_lines_draw(CGContextRef ctx, CGFontRef cg_font, const char* raw, double x, double y, double pt_size) {
    CTFontRef font = ui_test_scene_text_ct_font_(cg_font, pt_size, 0.0, false);

    double line_advance = CTFontGetAscent(font) + CTFontGetDescent(font) + CTFontGetLeading(font);
    double baseline = y + CTFontGetAscent(font);

    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[4] = { 0.0, 0.0, 0.0, 1.0 };
    CGColorRef black = CGColorCreate(space, components);
    CGColorSpaceRelease(space);

    NSDictionary* attributes = @{
        (id)kCTFontAttributeName: (__bridge id)font,
        (id)kCTForegroundColorAttributeName: (__bridge id)black,
    };

    NSArray* lines = [[NSString stringWithUTF8String:raw] componentsSeparatedByString:@"\n"];

    for (NSString* line_str in lines) {
        NSAttributedString* str = [[NSAttributedString alloc] initWithString:line_str attributes:attributes];
        CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)str);

        CGContextSetTextPosition(ctx, x, baseline);
        CTLineDraw(line, ctx);

        CFRelease(line);
        [str release];

        baseline += line_advance;
    }

    CGColorRelease(black);
    CFRelease(font);
}

static void ui_test_scene_text_lines_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_system_background);

    CGFontRef cg_font = ui_test_scene_text_cg_font(state->scenes, false);

    CGContextScaleCTM(ctx, to_px_from_pt, to_px_from_pt);
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));

    ui_test_scene_text_lines_draw(ctx, cg_font, ui_test_scene_text_lines_raw_13_, 32.0, 32.0, 13.0);
    ui_test_scene_text_lines_draw(ctx, cg_font, ui_test_scene_text_lines_raw_22_, 32.0, 140.0, 22.0);

    CGFontRelease(cg_font);

    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}

#endif

#define ui_test_text_many_count 900
#define ui_test_text_many_cols 21
#define ui_test_text_many_label_max 8

static uint32_t ui_test_text_many_items(struct ui_test_scene_text_item* out, char* labels, uint32_t label_stride) {
    for (uint32_t i = 0; i < ui_test_text_many_count; i++) {
        char* raw = labels + (uint64_t)i * label_stride;
        snprintf(raw, label_stride, "it%u", i);
        out[i] = (struct ui_test_scene_text_item) {
            .raw = raw,
            .x = 8.0 + (double)(i % ui_test_text_many_cols) * 48.0,
            .y = 8.0 + (double)(i / ui_test_text_many_cols) * 23.0,
            .pt_size = 9.0 + (double)(i % 7),
            .color = ui_label,
        };
    }
    return ui_test_text_many_count;
}

static struct ui_test_scene_state* ui_test_text_many_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    void* extra;
    struct ui_test_text_state* text = ui_test_text_state_alloc(scenes, mem, ui_test_text_many_count * (sizeof(struct ui_test_scene_text_item) + ui_test_text_many_label_max), &extra);
    struct ui_test_scene_text_item* items = extra;
    char* labels = (char*)(items + ui_test_text_many_count);

    text->items = items;
    text->item_count = ui_test_text_many_items(items, labels, ui_test_text_many_label_max);
    ui_test_text_state_font(text);
    return &text->base;
}

#define ui_test_text_long_pt 13.7529
#define ui_test_text_long_x 8.0
#define ui_test_text_long_y 8.0
#define ui_test_text_long_width 1008.0
#define ui_test_text_long_max 16001

static struct ui_node ui_test_scene_text_long_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    struct ui_test_text_state* text = (struct ui_test_text_state*)state;

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_system_background),
        .clip = true,
    });

    ui_text(&(struct ui_text) {
        .parent = root,
        .pos = ui_pos_xy(ui_test_text_long_x, ui_test_text_long_y),
        .size.width = ui_size_axis_fix(ui_test_text_long_width),
        .str = ui_str(text->text, ui_label, ui_test_text_long_pt),
    });

    return root;
}

#if cr_ios
static void ui_test_scene_text_long_pixels(struct ui_test_scene_state* state, uint32_t width, uint32_t height, float to_px_from_pt, float* out_rgba, uint32_t bytes_per_row) {
    struct ui_test_text_state* text = (struct ui_test_text_state*)state;

    CGContextRef ctx = ui_test_ref_begin_(width, height, ui_system_background);

    CGFontRef cg_font = ui_test_scene_text_cg_font(state->scenes, false);
    CTFontRef font = ui_test_scene_text_ct_font_(cg_font, ui_test_text_long_pt, 0.0, false);

    CGContextScaleCTM(ctx, to_px_from_pt, to_px_from_pt);
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));

    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[4] = { ui_label.value.x, ui_label.value.y, ui_label.value.z, ui_label.value.w };
    CGColorRef color = CGColorCreate(space, components);
    CGColorSpaceRelease(space);

    NSDictionary* attributes = @{
        (id)kCTFontAttributeName: (__bridge id)font,
        (id)kCTForegroundColorAttributeName: (__bridge id)color,
    };
    NSAttributedString* str = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:text->text] attributes:attributes];
    CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString((CFAttributedStringRef)str);

    double ascent = CTFontGetAscent(font);
    double line_advance = ascent + CTFontGetDescent(font) + CTFontGetLeading(font);
    double baseline = ui_test_text_long_y + ascent;
    double bottom = (double)height / to_px_from_pt;
    CFIndex length = (CFIndex)str.length;
    CFIndex start = 0;

    while (start < length && baseline - ascent < bottom) {
        CFIndex count = CTTypesetterSuggestLineBreak(typesetter, start, ui_test_text_long_width);
        assert(count > 0);
        CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(start, count));
        CGContextSetTextPosition(ctx, ui_test_text_long_x, baseline);
        CTLineDraw(line, ctx);
        CFRelease(line);

        start += count;
        baseline += line_advance;
    }

    CFRelease(typesetter);
    [str release];
    CGColorRelease(color);
    CFRelease(font);
    CGFontRelease(cg_font);

    ui_test_ref_end_(ctx, width, height, out_rgba, bytes_per_row);
}
#endif

static struct ui_test_scene_state* ui_test_text_long_create(struct ui_test_scenes* scenes, struct cr_mem* mem, const void* ctx) {
    void* extra;
    struct ui_test_text_state* text = ui_test_text_state_alloc(scenes, mem, ui_test_text_long_max, &extra);
    char* buf = extra;

    static const char* const words[8] = { "signal", "measure", "kerning", "layout", "grapheme", "shaping", "cluster", "baseline" };
    uint32_t offset = 0;
    uint32_t i = 0;
    while (offset < ui_test_text_long_max - 16) {
        int n = snprintf(buf + offset, ui_test_text_long_max - offset, "%s ", words[i % 8]);
        offset += (uint32_t)n;
        i++;
    }

    text->text = buf;
    return &text->base;
}

static const struct ui_test_scene ui_test_scenes_text_table_[] = {
    {
        .label = "text", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0016f,
        .tile_threshold = 0.07f,
        .alarm_allowance = 512,
        ui_test_text_scene(ui_test_scene_text_items_),
    },
    {
        .label = "text-2x", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.0012f,
        .tile_threshold = 0.09f,
        .alarm_allowance = 1536,
        ui_test_text_scene(ui_test_scene_text_items_),
    },
    {
        .label = "text-weights", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0024f,
        .tile_threshold = 0.024f,
        .alarm_allowance = 512,
        ui_test_text_scene(ui_test_scene_text_weights_items_),
    },
    {
        .label = "text-inter", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0015f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 128,
        ui_test_text_scene(ui_test_scene_text_inter_items_),
    },
    {
        .label = "text-inter-2x", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.001f,
        .tile_threshold = 0.06f,
        .alarm_allowance = 512,
        ui_test_text_scene(ui_test_scene_text_inter_items_),
    },
    {
        .label = "text-lines", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0014f,
        .tile_threshold = 0.04f,
        .alarm_allowance = 512,
        .build = ui_test_scene_text_lines_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_text_lines_pixels),
    },
    {
        .label = "text-fractional", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 512.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0014f,
        .tile_threshold = 0.03f,
        .alarm_allowance = 256,
        ui_test_text_scene(ui_test_scene_text_fractional_items_),
    },
    {
        .label = "text-atlas", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 1024.0, 608.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.0008f,
        .tile_threshold = 0.02f,
        .alarm_allowance = 192,
        ui_test_text_atlas_scene(0),
    },
    {
        .label = "text-atlas-mono", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 1024.0, 608.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.0008f,
        .tile_threshold = 0.012f,
        .alarm_allowance = 96,
        ui_test_text_atlas_scene(1),
    },
    {
        .label = "text-atlas-small", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 512.0, 240.0 }, .to_px_from_pt = 1.0f,
        .mean_threshold = 0.0016f,
        .tile_threshold = 0.014f,
        .alarm_allowance = 64,
        ui_test_text_atlas_scene(2),
    },
    {
        .label = "text-many", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 1024.0, 1024.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.003f,
        .tile_threshold = 0.07f,
        .alarm_allowance = 8192,
        .create = ui_test_text_many_create,
        .build = ui_test_text_state_build,
        .pixels = ui_test_scene_pixels(ui_test_text_state_pixels),
    },
    {
        .label = "text-long", .group = "text",
        .capture = ui_test_scene_capture_pixels,
        .size = { 1024.0, 1024.0 }, .to_px_from_pt = 2.0f,
        .mean_threshold = 0.004f,
        .tile_threshold = 0.07f,
        .alarm_allowance = 8192,
        .create = ui_test_text_long_create,
        .build = ui_test_scene_text_long_build,
        .pixels = ui_test_scene_pixels(ui_test_scene_text_long_pixels),
    },
};

const struct ui_test_scene* ui_test_scenes_text_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = cr_count(ui_test_scenes_text_table_);
    return ui_test_scenes_text_table_;
}
