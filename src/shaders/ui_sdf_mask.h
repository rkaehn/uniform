#pragma once

#include "ui_sdf_shared.h"

[[vk::binding(ui_bind_group_0_sdf, 0)]] StructuredBuffer<ui_sdf_mask_prim> group_0_sdf;

#define ui_sdf_mask_paste_(a, b) a##b
#define ui_sdf_mask_paste(a, b) ui_sdf_mask_paste_(a, b)
#define ui_sdf_mask_prim_distance ui_sdf_mask_paste(ui_sdf_mask_prefix, _sdf_mask_prim_distance)
#define ui_sdf_mask_coverage ui_sdf_mask_paste(ui_sdf_mask_prefix, _sdf_mask_coverage)

static float ui_sdf_mask_prim_distance(uint32_t index, float2 p) {
    float4 pos = float4(group_0_sdf[index].pos.x, group_0_sdf[index].pos.y, group_0_sdf[index].pos.z, group_0_sdf[index].pos.w);
    float4 cell_x = float4(group_0_sdf[index].cell_x.x, group_0_sdf[index].cell_x.y, group_0_sdf[index].cell_x.z, group_0_sdf[index].cell_x.w);
    float4 cell_y = float4(group_0_sdf[index].cell_y.x, group_0_sdf[index].cell_y.y, group_0_sdf[index].cell_y.z, group_0_sdf[index].cell_y.w);

    float2 rel = p - pos.xy;
    float d = ui_sdf_analytic_prim_sharp(pos, p).x;

    cr_unroll
    for (uint32_t i = 0; i < 4; i++) {
        float mx = (i == 0 || i == 3) ? -1.0f : 1.0f;
        float my = i < 2 ? -1.0f : 1.0f;
        float2 cell = float2(cell_x[i], cell_y[i]);
        if (cell.x <= 0.0f || cell.y <= 0.0f) {
            continue;
        }
        float2 q = float2(rel.x * mx, rel.y * my);
        float2 cell_origin = pos.zw - cell;
        if (q.x > cell_origin.x && q.y > cell_origin.y) {
            float4 c0 = float4(group_0_sdf[index].coef[2 * i].x, group_0_sdf[index].coef[2 * i].y,
                               group_0_sdf[index].coef[2 * i].z, group_0_sdf[index].coef[2 * i].w);
            float4 c1 = float4(group_0_sdf[index].coef[2 * i + 1].x, group_0_sdf[index].coef[2 * i + 1].y,
                               group_0_sdf[index].coef[2 * i + 1].z, group_0_sdf[index].coef[2 * i + 1].w);
            float d_curve = ui_sdf_analytic_corner_curve(q, cell_origin, cell, c0, c1, ui_sdf_mask_range).x;
            d = max(d, d_curve);
        }
    }

    return d;
}

static float ui_sdf_mask_coverage(float ref, float2 p) {
    if (ref < 0.5f) {
        return 1.0f;
    }
    uint32_t index = (uint32_t)(ref + 0.5f);
    float d = ui_sdf_mask_prim_distance(index, p);
    if (group_0_sdf[index].meta.x > 1.5f) {
        d = max(d, -ui_sdf_mask_prim_distance(index + 1, p));
    }
    return saturate(0.5f - d);
}
