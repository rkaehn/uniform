#include "ui_color.h"

enum ui_color_primaries {
    ui_color_primaries_srgb,
    ui_color_primaries_p3,

    ui_color_primaries_count,
};

static const float3x3 ui_color_matrices[ui_color_primaries_count][ui_color_primaries_count] = {
    [ui_color_primaries_srgb][ui_color_primaries_srgb] = { .columns = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    } },
    [ui_color_primaries_srgb][ui_color_primaries_p3] = { .columns = {
        { 0.822461969f, 0.033194199f, 0.017082631f },
        { 0.177538031f, 0.966805801f, 0.072397441f },
        { 0.0f, 0.0f, 0.910519929f },
    } },
    [ui_color_primaries_p3][ui_color_primaries_srgb] = { .columns = {
        { 1.224940176f, -0.042056955f, -0.019637555f },
        { -0.224940176f, 1.042056955f, -0.078636046f },
        { 0.0f, 0.0f, 1.098273600f },
    } },
    [ui_color_primaries_p3][ui_color_primaries_p3] = { .columns = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    } },
};

static enum ui_color_primaries ui_color_space_primaries(enum ui_color_space color_space) {
    switch (color_space) {
    case ui_color_space_srgb:
    case ui_color_space_srgb_linear:
        return ui_color_primaries_srgb;
    case ui_color_space_display_p3:
    case ui_color_space_display_p3_linear:
        return ui_color_primaries_p3;
    }
}

static bool ui_color_space_is_linear(enum ui_color_space color_space) {
    switch (color_space) {
    case ui_color_space_srgb:
    case ui_color_space_display_p3:
        return false;
    case ui_color_space_srgb_linear:
    case ui_color_space_display_p3_linear:
        return true;
    }
}

struct ui_convert ui_convert_make(enum ui_color_space src, enum ui_color_space dst) {
    enum ui_color_primaries src_primaries = ui_color_space_primaries(src);
    enum ui_color_primaries dst_primaries = ui_color_space_primaries(dst);

    if (src_primaries == dst_primaries && ui_color_space_is_linear(src) == ui_color_space_is_linear(dst)) {
        return (struct ui_convert) {
            .matrix = ui_color_matrices[src_primaries][dst_primaries],
        };
    }

    return (struct ui_convert) {
        .needed = true,
        .decode = ui_color_space_is_linear(src) == false,
        .encode = ui_color_space_is_linear(dst) == false,
        .matrix_any = src_primaries != dst_primaries,
        .matrix = ui_color_matrices[src_primaries][dst_primaries],
    };
}

float3 ui_convert_apply_float3(const struct ui_convert* convert, float3 v) {
    if (convert->decode) {
        v = to_linear_srgb_from_srgb_signed_float3(v);
    }
    if (convert->matrix_any) {
        v = mul_float3x3_float3(convert->matrix, v);
    }
    if (convert->encode) {
        v = to_srgb_from_linear_srgb_signed_float3(v);
    }
    return v;
}

struct ui_convert ui_convert_make_always(enum ui_color_space src, enum ui_color_space dst) {
    enum ui_color_primaries src_primaries = ui_color_space_primaries(src);
    enum ui_color_primaries dst_primaries = ui_color_space_primaries(dst);

    return (struct ui_convert) {
        .needed = true,
        .decode = ui_color_space_is_linear(src) == false,
        .encode = ui_color_space_is_linear(dst) == false,
        .matrix_any = true,
        .matrix = ui_color_matrices[src_primaries][dst_primaries],
    };
}

float4 ui_to_compositing_from_color(struct ui_color color) {
    if (color.color_space != ui_color_space_srgb) {
        struct ui_convert convert = ui_convert_make(color.color_space, ui_color_space_srgb);
        color.value.xyz = ui_convert_apply_float3(&convert, color.value.xyz);
    }
    return premultiply_float4(color.value);
}
