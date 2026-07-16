#pragma once

#include "cr.h"
#include "ui.h"
#include "ui_shared.h"

struct ui;

struct ui_convert {
    bool needed;
    bool decode;
    bool encode;
    bool matrix_any;
    float3x3 matrix;
};

cr_hdef void ui_convert_write(uint32_t* flags, float4* col_x, float4* col_y, float4* col_z, const struct ui_convert* convert) {
    *flags |= ui_graphic_flag_cs_convert |
              (convert->decode ? ui_graphic_flag_cs_decode : 0u) |
              (convert->encode ? ui_graphic_flag_cs_encode : 0u);
    col_x->xyz = convert->matrix.columns[0];
    col_y->xyz = convert->matrix.columns[1];
    col_z->xyz = convert->matrix.columns[2];
}

struct ui_convert ui_convert_make(enum ui_color_space src, enum ui_color_space dst);
struct ui_convert ui_convert_make_always(enum ui_color_space src, enum ui_color_space dst);
float3 ui_convert_apply_float3(const struct ui_convert* convert, float3 v);
float4 ui_to_compositing_from_color(struct ui_color color);
