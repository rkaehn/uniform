#pragma once

#include "cr.h"
#include "gpu.h"
#include "ui.h"

struct ui;
struct ui_graphic;
struct ui_glyph;
struct ui_sdf;

extern _Thread_local struct ui* ui_custom_on_render_ctx;

void ui_render_init(struct ui* ui);
void ui_render_deinit(struct ui* ui);
struct gpu_bind_group* ui_render_texture_bind_group(struct ui* ui, struct gpu_texture* texture, bool clamp, bool nearest);
struct ui_sdf* ui_render_sdf(void);
void ui_render_retain_graphic(struct ui* ui, struct ui_graphic* graphic);
void ui_render_add_glyph(struct ui* ui, const struct ui_glyph* glyph, int2 pen_px, float4 color);
bool ui_render_last_direct_(void);
