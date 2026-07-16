#include "cr.h"
#include "ui.h"
#include "ui_tests.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#if cr_ios

#import <Foundation/Foundation.h>

@interface SwiftUIShapesBridge : NSObject
+ (void)renderWithWidth:(uint32_t)width height:(uint32_t)height prims:(const struct ui_sdf_prim*)prims primCount:(uint32_t)primCount background:(const float*)background tint:(const float*)tint primUnions:(const uint32_t*)primUnions variant:(uint32_t)variant appearance:(uint32_t)appearance patternRects:(const struct ui_test_glass_pattern_rect*)patternRects patternRectCount:(uint32_t)patternRectCount edrRects:(const struct ui_test_glass_pattern_rect*)edrRects edrRectCount:(uint32_t)edrRectCount toPxFromPt:(CGFloat)toPxFromPt;
@end

#endif

#define ui_test_glass_rfr_max_rects 520

struct ui_test_glass_rfr_pattern {
    float4 base;
    uint32_t rect_count;
    struct ui_test_glass_pattern_rect rects[ui_test_glass_rfr_max_rects];
};

struct ui_test_glass_rfr_spec {
    bool vertical;
    double period;
    double phase;
    double step_at;
    bool is_step;
    bool is_flat;
    float4 lo;
    float4 hi;
};

static struct ui_test_glass_rfr_spec ui_test_glass_rfr_specs(uint32_t index, double shape_left) {
    float4 lo = make_float4(0.18f, 0.18f, 0.18f, 1.0f);
    float4 hi = make_float4(0.78f, 0.78f, 0.78f, 1.0f);

    switch (index) {
    case 0:  return (struct ui_test_glass_rfr_spec) { .is_flat = true, .lo = make_float4(0.40f, 0.40f, 0.40f, 1.0f) };
    case 1:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 2:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 4.0, .lo = lo, .hi = hi };
    case 3:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 8.0, .lo = lo, .hi = hi };
    case 4:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 12.0, .lo = lo, .hi = hi };
    case 5:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 16.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 6:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 16.0, .phase = 4.0, .lo = lo, .hi = hi };
    case 7:  return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 8.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 8:  return (struct ui_test_glass_rfr_spec) { .vertical = false, .period = 32.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 9:  return (struct ui_test_glass_rfr_spec) { .vertical = false, .period = 32.0, .phase = 8.0, .lo = lo, .hi = hi };
    case 10: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = -1.0, .lo = lo, .hi = hi };
    case 11: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = shape_left + 2.0, .lo = lo, .hi = hi };
    case 12: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = shape_left + 6.0, .lo = lo, .hi = hi };
    case 13: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = shape_left - 2.0, .lo = lo, .hi = hi };
    case 14: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(1.0f, 0.0f, 0.0f, 1.0f), .hi = make_float4(0.0f, 0.0f, 1.0f, 1.0f) };
    case 15: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };
    case 16: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 4.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 17: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 2.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 18: return (struct ui_test_glass_rfr_spec) { .vertical = false, .period = 6.0, .phase = 0.0, .lo = lo, .hi = hi };
    case 19: return (struct ui_test_glass_rfr_spec) { .vertical = false, .period = 6.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };

    case 20: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.35f, 0.35f, 0.35f, 1.0f), .hi = make_float4(0.58f, 0.58f, 0.58f, 1.0f) };
    case 21: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.25f, 0.25f, 0.25f, 1.0f), .hi = make_float4(0.68f, 0.68f, 0.68f, 1.0f) };
    case 22: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.15f, 0.15f, 0.15f, 1.0f), .hi = make_float4(0.78f, 0.78f, 0.78f, 1.0f) };
    case 23: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.55f, 0.55f, 0.55f, 1.0f) };
    case 24: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 32.0, .phase = 0.0, .lo = make_float4(0.45f, 0.45f, 0.45f, 1.0f), .hi = make_float4(0.95f, 0.95f, 0.95f, 1.0f) };
    case 25: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 8.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };
    case 26: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 16.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };
    case 27: return (struct ui_test_glass_rfr_spec) { .vertical = true, .period = 64.0, .phase = 0.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };
    case 28: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = -1.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.88f, 0.88f, 0.88f, 1.0f) };
    case 29: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = -1.0, .lo = make_float4(0.88f, 0.88f, 0.88f, 1.0f), .hi = make_float4(0.05f, 0.05f, 0.05f, 1.0f) };
    default: return (struct ui_test_glass_rfr_spec) { .is_step = true, .step_at = -1.0, .lo = make_float4(0.05f, 0.05f, 0.05f, 1.0f), .hi = make_float4(0.55f, 0.55f, 0.55f, 1.0f) };
    }
}

static uint32_t ui_test_glass_content_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static double ui_test_glass_content_hash01(uint32_t x) {
    return (double)ui_test_glass_content_hash(x) / 4294967296.0;
}

static double ui_test_glass_snap_px(double v, double to_px_from_pt) {
    return floor(v * to_px_from_pt + 0.5) / to_px_from_pt;
}

static bool ui_test_glass_on_px_grid(double v, double to_px_from_pt) {
    double px = v * to_px_from_pt;
    return fabs(px - floor(px + 0.5)) < 1e-9;
}

static void ui_test_glass_content_rect(struct ui_test_glass_rfr_pattern* out, double x, double y, double w, double h, double g) {
    if (out->rect_count >= ui_test_glass_rfr_max_rects - 8) {
        return;
    }
    out->rects[out->rect_count++] = (struct ui_test_glass_pattern_rect) {
        .origin = make_double2(x, y),
        .size = make_double2(w, h),
        .color = make_float4((float)g, (float)g, (float)g, 1.0f),
    };
}

static void ui_test_glass_rfr_content_make(uint32_t index, double2 size, double to_px_from_pt, struct ui_test_glass_rfr_pattern* out) {
    double W = size.x;
    double H = size.y;

    if (index == 31) {

        out->base = make_float4(0.05f, 0.05f, 0.05f, 1.0f);
        for (double x = 0.0; x < W; x += 2.0) {
            double g = 0.05 + 0.83 * ((x + 1.0) / W);
            ui_test_glass_content_rect(out, x, 0.0, min_double(2.0, W - x), H, g);
        }
        return;
    }

    if (index == 32) {

        out->base = make_float4(0.45f, 0.45f, 0.45f, 1.0f);
        uint32_t nx = (uint32_t)(W / 64.0), ny = (uint32_t)(H / 64.0);
        for (uint32_t j = 0; j < ny; j++) {
            for (uint32_t i = 0; i < nx; i++) {
                uint32_t k = 32000009u + j * 512u + i;
                ui_test_glass_content_rect(out, i * 64.0, j * 64.0, 64.0, 64.0,
                                           0.10 + 0.80 * ui_test_glass_content_hash01(k));
            }
        }
        nx = (uint32_t)(W / 32.0); ny = (uint32_t)(H / 32.0);
        for (uint32_t j = 0; j < ny; j++) {
            for (uint32_t i = 0; i < nx; i++) {
                uint32_t k = 64000031u + j * 512u + i;
                if ((ui_test_glass_content_hash(k) & 3u) != 0u) {
                    continue;
                }
                ui_test_glass_content_rect(out, i * 32.0, j * 32.0, 32.0, 32.0,
                                           0.10 + 0.80 * ui_test_glass_content_hash01(k + 7u));
            }
        }
        nx = (uint32_t)(W / 8.0); ny = (uint32_t)(H / 8.0);
        for (uint32_t j = 0; j < ny; j++) {
            for (uint32_t i = 0; i < nx; i++) {
                uint32_t k = 96000053u + j * 512u + i;
                if ((ui_test_glass_content_hash(k) & 63u) != 0u) {
                    continue;
                }
                ui_test_glass_content_rect(out, i * 8.0, j * 8.0, 8.0, 8.0,
                                           0.05 + 0.90 * ui_test_glass_content_hash01(k + 13u));
            }
        }
        return;
    }

    if (index == 33) {

        out->base = make_float4(0.93f, 0.93f, 0.93f, 1.0f);
        uint32_t row = 0;
        for (double y = 24.0; y + 9.0 <= H - 24.0; y += 24.0, row++) {
            double x = 28.0;
            uint32_t w = 0;
            while (x < W - 34.0) {

                double wl = floor(24.0 + 40.0 * ui_test_glass_content_hash01(33000017u + row * 512u + w));
                if (x + wl > W - 28.0) {
                    wl = W - 28.0 - x;
                }
                ui_test_glass_content_rect(out, x, y, wl, 9.0, 0.20);
                x += wl + 10.0;
                w++;
            }
        }
        return;
    }

    out->base = make_float4(0.88f, 0.88f, 0.88f, 1.0f);
    uint32_t cols = (uint32_t)(W / 240.0);
    if (cols < 2) {
        cols = 2;
    }
    uint32_t rows = (uint32_t)(H / 128.0);
    double cw = W / (double)cols, ch = H / (double)rows;
    for (uint32_t j = 0; j < rows; j++) {
        for (uint32_t i = 0; i < cols; i++) {
            double cx = ui_test_glass_snap_px(i * cw, to_px_from_pt) + 12.0, cy = ui_test_glass_snap_px(j * ch, to_px_from_pt) + 12.0;
            double w = ui_test_glass_snap_px(cw, to_px_from_pt) - 24.0, h = ui_test_glass_snap_px(ch, to_px_from_pt) - 24.0;
            ui_test_glass_content_rect(out, cx, cy, w, h, 0.98);
            ui_test_glass_content_rect(out, cx + 14.0, cy + 14.0, floor(w * 0.5), 8.0, 0.25);
            ui_test_glass_content_rect(out, cx + 14.0, cy + 30.0, floor(w * 0.7), 6.0, 0.60);
            ui_test_glass_content_rect(out, cx + w - 34.0, cy + 14.0, 22.0, 22.0, 0.45);
        }
    }
}

static void ui_test_glass_rfr_pattern_make(uint32_t index, double2 size, double shape_left, double to_px_from_pt, struct ui_test_glass_rfr_pattern* out) {
    if (index >= 31) {
        out->rect_count = 0;
        ui_test_glass_rfr_content_make(index, size, to_px_from_pt, out);
        return;
    }

    struct ui_test_glass_rfr_spec spec = ui_test_glass_rfr_specs(index, shape_left);

    out->base = spec.lo;
    out->rect_count = 0;

    if (spec.is_flat) {
        return;
    }

    if (spec.is_step) {
        double at = spec.step_at < 0.0 ? floor(size.x * 0.5) : spec.step_at;
        out->rects[out->rect_count++] = (struct ui_test_glass_pattern_rect) {
            .origin = make_double2(at, 0.0),
            .size = make_double2(size.x - at, size.y),
            .color = spec.hi,
        };
        return;
    }

    double extent = spec.vertical ? size.x : size.y;
    double cross = spec.vertical ? size.y : size.x;
    double half = spec.period * 0.5;

    for (double x0 = spec.phase - spec.period; x0 < extent; x0 += spec.period) {
        double lo_edge = max_double(x0, 0.0);
        double hi_edge = min_double(x0 + half, extent);
        if (hi_edge <= lo_edge) {
            continue;
        }
        assert(out->rect_count < ui_test_glass_rfr_max_rects);
        out->rects[out->rect_count++] = (struct ui_test_glass_pattern_rect) {
            .origin = spec.vertical ? make_double2(lo_edge, 0.0) : make_double2(0.0, lo_edge),
            .size = spec.vertical ? make_double2(hi_edge - lo_edge, cross) : make_double2(cross, hi_edge - lo_edge),
            .color = spec.hi,
        };
    }
}

static uint32_t ui_test_glass_pattern_rects(uint32_t index, double2 size_pt, double shape_left, double to_px_from_pt, float4* out_base, struct ui_test_glass_pattern_rect* out_rects, uint32_t capacity) {
    struct ui_test_glass_rfr_pattern pattern;
    ui_test_glass_rfr_pattern_make(index, size_pt, shape_left, to_px_from_pt, &pattern);
    *out_base = pattern.base;
    uint32_t count = min_uint32(pattern.rect_count, capacity);
    memcpy(out_rects, pattern.rects, count * sizeof(pattern.rects[0]));
    for (uint32_t i = 0; i < count; i++) {
        assert(ui_test_glass_on_px_grid(out_rects[i].origin.x, to_px_from_pt) && ui_test_glass_on_px_grid(out_rects[i].origin.y, to_px_from_pt));
        assert(ui_test_glass_on_px_grid(out_rects[i].size.x, to_px_from_pt) && ui_test_glass_on_px_grid(out_rects[i].size.y, to_px_from_pt));
    }
    return count;
}

static double ui_test_glass_shape_left(const struct ui_test_capture_scene* s) {
    if (s->prim_count == 0) {
        return 0.0;
    }
    const struct ui_sdf_prim* prim = &s->prims[0];
    return prim->type == ui_sdf_prim_type_circle ? prim->circle.center.x - prim->circle.radius : prim->rect.origin.x;
}

static uint32_t ui_test_glass_edr_rects(const struct ui_test_capture_scene* scene, double to_px_from_pt, struct ui_test_glass_pattern_rect* out, uint32_t capacity) {
    uint32_t count = 0;
    double w = scene->canvas_w;
    double h = scene->canvas_h;
    float boost = scene->edr_boost;

    if (scene->kind == ui_test_capture_kind_edr_ramp) {
        static const float levels[] = { 0.05f, 0.1f, 0.2f, 0.35f, 0.5f, 0.7f, 0.85f, 1.0f, 1.1f, 1.25f, 1.4f, 1.6f, 1.8f, 2.0f, 2.5f, 3.0f };
        double sw = w / (double)cr_count(levels);
        for (uint32_t i = 0; i < cr_count(levels) && count < capacity; i++) {
            out[count++] = (struct ui_test_glass_pattern_rect) {
                .origin = make_double2(i * sw, 0.0),
                .size = make_double2(sw, h),
                .color = make_float4(levels[i], levels[i], levels[i], 1.0f),
            };
        }
        return count;
    }

    if (strcmp(scene->bg_tag, "edrpatch") == 0) {
        static const double spots[][2] = { { 0.12, 0.12 }, { 0.88, 0.12 }, { 0.12, 0.88 }, { 0.88, 0.88 } };
        for (uint32_t i = 0; i < cr_count(spots) && count < capacity; i++) {
            out[count++] = (struct ui_test_glass_pattern_rect) {
                .origin = make_double2(ui_test_glass_snap_px(spots[i][0] * w, to_px_from_pt) - 32.0, ui_test_glass_snap_px(spots[i][1] * h, to_px_from_pt) - 32.0),
                .size = make_double2(64.0, 64.0),
                .color = make_float4(boost, boost, boost, 1.0f),
            };
        }
        return count;
    }

    if (strcmp(scene->bg_tag, "edrstripe") == 0) {
        for (double x = 16.0; x + 32.0 < w && count < capacity; x += 64.0) {
            out[count++] = (struct ui_test_glass_pattern_rect) {
                .origin = make_double2(x, 0.0),
                .size = make_double2(32.0, h),
                .color = make_float4(boost, boost, boost, 1.0f),
            };
        }
        return count;
    }

    if (strcmp(scene->bg_tag, "edrrim") == 0 && capacity > 0) {
        double x = w * 0.5 - scene->md * 0.5 - 48.0;
        out[count++] = (struct ui_test_glass_pattern_rect) {
            .origin = make_double2(x, h * 0.5 - 48.0),
            .size = make_double2(96.0, 96.0),
            .color = make_float4(boost, boost, boost, 1.0f),
        };
    }
    for (uint32_t i = 0; i < count; i++) {
        assert(ui_test_glass_on_px_grid(out[i].origin.x, to_px_from_pt) && ui_test_glass_on_px_grid(out[i].origin.y, to_px_from_pt));
        assert(ui_test_glass_on_px_grid(out[i].size.x, to_px_from_pt) && ui_test_glass_on_px_grid(out[i].size.y, to_px_from_pt));
    }
    return count;
}

#define ui_test_capture_max 1400

struct ui_test_glass_build {
    struct ui_test_capture_scene* scenes;
    uint32_t count;
    double to_px_from_pt;
    const char* group;
    struct ui_test_capture_scene cur;
};

static void cap_reset(struct ui_test_glass_build* cap) {
    memset(&cap->cur, 0, sizeof(cap->cur));
    cap->cur.slider = 0.5f;
    cap->cur.pattern = -1;
    cap->cur.background = make_float4(0.4f, 0.4f, 0.4f, 1.0f);
    snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "bg40");
    snprintf(cap->cur.profile, sizeof(cap->cur.profile), "cir");
}

static uint16_t cap_canvas_dim(float content) {
    float k = content + 256.0f;
    k = ceilf(k / 64.0f) * 64.0f;
    return (uint16_t)min_float(k, 1032.0f);
}

static void cap_circle(struct ui_test_glass_build* cap, float md) {
    uint16_t k = cap_canvas_dim(md);
    cap->cur.canvas_w = k;
    cap->cur.canvas_h = k;
    cap->cur.md = md;
    cap->cur.prim_count = 1;
    cap->cur.prims[0] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_circle,
        .circle = { .center = { k * 0.5, k * 0.5 }, .radius = md * 0.5 },
    };
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "c%.0f", md);
}

static void cap_rect_tag(struct ui_test_glass_build* cap, double w, double h, float radius, bool continuous, const char* tag) {
    cap->cur.canvas_w = cap_canvas_dim((float)w);
    cap->cur.canvas_h = cap_canvas_dim((float)h);
    cap->cur.md = (float)min_double(w, h);
    cap->cur.radius = radius;
    snprintf(cap->cur.profile, sizeof(cap->cur.profile), continuous ? "con" : "cir");
    cap->cur.prim_count = 1;
    cap->cur.prims[0] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_rect,
        .rect = {
            .origin = { (cap->cur.canvas_w - w) * 0.5, (cap->cur.canvas_h - h) * 0.5 },
            .width = w, .height = h, .radius = radius, .continuous = continuous,
        },
    };
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "%s", tag);
}

static void cap_rect(struct ui_test_glass_build* cap, float md, float radius, bool continuous) {
    char tag[16];
    snprintf(tag, sizeof(tag), "re%.0fr%.0f%s", md, radius, continuous ? "c" : "");
    cap_rect_tag(cap, md, md, radius, continuous, tag);
}

static void cap_capsule(struct ui_test_glass_build* cap, float w, float h) {
    char tag[16];
    snprintf(tag, sizeof(tag), "bar%.0fx%.0f", w, h);
    cap_rect_tag(cap, w, h, h * 0.5f, false, tag);
}

static void cap_bg_gray(struct ui_test_glass_build* cap, float v) {
    cap->cur.background = make_float4(v, v, v, 1.0f);
    snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "bg%02.0f", (double)(v * 100.0f));
}

static void cap_bg_rgb(struct ui_test_glass_build* cap, float r, float g, float b) {
    cap->cur.background = make_float4(r, g, b, 1.0f);
    snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "bg%02x%02x%02x",
             (uint32_t)(r * 255.0f + 0.5f), (uint32_t)(g * 255.0f + 0.5f), (uint32_t)(b * 255.0f + 0.5f));
}

static void cap_tint(struct ui_test_glass_build* cap, float r, float g, float b, float a) {
    cap->cur.tint = make_float4(r, g, b, a);
    snprintf(cap->cur.tint_tag, sizeof(cap->cur.tint_tag), "t%02x%02x%02x%02x",
             (uint32_t)(r * 255.0f + 0.5f), (uint32_t)(g * 255.0f + 0.5f),
             (uint32_t)(b * 255.0f + 0.5f), (uint32_t)(a * 255.0f + 0.5f));
}

static void cap_union_pair_circles(struct ui_test_glass_build* cap, float md, float gap) {
    float r = md * 0.5f;
    float span = md * 2.0f + gap;
    uint16_t k = cap_canvas_dim(span);
    cap->cur.canvas_w = k;
    cap->cur.canvas_h = cap_canvas_dim(md);
    cap->cur.md = md;
    cap->cur.prim_count = 2;
    double cy = cap->cur.canvas_h * 0.5;
    double d = md + gap;
    cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { k * 0.5 - d * 0.5, cy }, .radius = r } };
    cap->cur.prims[1] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { k * 0.5 + d * 0.5, cy }, .radius = r } };
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "u-cc%.0f", md);
    snprintf(cap->cur.union_tag, sizeof(cap->cur.union_tag), "g%.0f", gap);
}

static void cap_union_siblings(struct ui_test_glass_build* cap) {
    cap->cur.union_groups[0] = 0;
    cap->cur.union_groups[1] = 1;
    cap->cur.union_groups[2] = 2;
    size_t len = strlen(cap->cur.union_tag);
    snprintf(cap->cur.union_tag + len, sizeof(cap->cur.union_tag) - len, "s");
}

static void cap_union_named(struct ui_test_glass_build* cap, const char* shape_tag, const char* union_tag) {
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "%s", shape_tag);
    snprintf(cap->cur.union_tag, sizeof(cap->cur.union_tag), "%s", union_tag);
}

static void cap_commit_named(struct ui_test_glass_build* cap, const char* name) {
    for (uint32_t i = 0; i < cap->cur.prim_count; i++) {
        const struct ui_sdf_prim* prim = &cap->cur.prims[i];
        double scale = cap->to_px_from_pt;
        if (prim->type == ui_sdf_prim_type_circle) {
            assert(ui_test_glass_on_px_grid(prim->circle.center.x - prim->circle.radius, scale));
            assert(ui_test_glass_on_px_grid(prim->circle.center.y - prim->circle.radius, scale));
            assert(ui_test_glass_on_px_grid(prim->circle.radius * 2.0, scale));
        } else {
            assert(ui_test_glass_on_px_grid(prim->rect.origin.x, scale) && ui_test_glass_on_px_grid(prim->rect.origin.y, scale));
            assert(ui_test_glass_on_px_grid(prim->rect.width, scale) && ui_test_glass_on_px_grid(prim->rect.height, scale));
        }
    }
    for (uint32_t i = 0; i < cap->count; i++) {
        if (strcmp(cap->scenes[i].name, name) == 0) {
            return;
        }
    }
    assert(cap->count < ui_test_capture_max);
    struct ui_test_capture_scene* s = &cap->scenes[cap->count++];
    *s = cap->cur;
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->group = cap->group;
}

static void cap_commit(struct ui_test_glass_build* cap) {
    char slider_tag[8] = "";
    if (fabsf(cap->cur.slider - 0.5f) > 1e-3f) {
        if (cap->cur.slider < 0.01f) {
            snprintf(slider_tag, sizeof(slider_tag), "-s0");
        } else if (cap->cur.slider > 0.99f) {
            snprintf(slider_tag, sizeof(slider_tag), "-s1");
        } else {
            snprintf(slider_tag, sizeof(slider_tag), "-s%03.0f", (double)(cap->cur.slider * 100.0f));
        }
    }

    char union_part[28] = "";
    if (cap->cur.union_tag[0] != 0) {
        snprintf(union_part, sizeof(union_part), "%s", cap->cur.union_tag);
    }

    char canvas_part[16];
    if (cap->cur.canvas_w == cap->cur.canvas_h) {
        snprintf(canvas_part, sizeof(canvas_part), "-k%u", cap->cur.canvas_w);
    } else {
        snprintf(canvas_part, sizeof(canvas_part), "-k%ux%u", cap->cur.canvas_w, cap->cur.canvas_h);
    }

    char bg_part[28];
    if (cap->cur.pattern >= 0) {
        snprintf(bg_part, sizeof(bg_part), "-p%02d", cap->cur.pattern);
    } else {
        snprintf(bg_part, sizeof(bg_part), "-%s", cap->cur.bg_tag);
    }

    char tint_part[24] = "";
    if (cap->cur.tint.w > 0.0f) {
        snprintf(tint_part, sizeof(tint_part), "-%s", cap->cur.tint_tag);
    }

    char edr_part[8] = "";
    if (cap->cur.edr_boost > 0.0f) {
        snprintf(edr_part, sizeof(edr_part), "-e%02.0f", (double)(cap->cur.edr_boost * 10.0f));
    }

    char name[112];
    snprintf(name, sizeof(name), "%s%s%s-%s%s%s%s%s%s",
             cap->cur.shape, union_part, canvas_part,
             cap->cur.regular ? "rg" : "cr", cap->cur.light ? "lt" : "dk",
             slider_tag, bg_part, tint_part, edr_part);
    cap_commit_named(cap, name);
}

static const float cap_color_sweep_[][3] = {
    { 0.00f, 0.00f, 0.00f }, { 0.05f, 0.05f, 0.05f }, { 0.10f, 0.10f, 0.10f }, { 0.18f, 0.18f, 0.18f },
    { 0.28f, 0.28f, 0.28f }, { 0.40f, 0.40f, 0.40f }, { 0.52f, 0.52f, 0.52f }, { 0.65f, 0.65f, 0.65f },
    { 0.78f, 0.78f, 0.78f }, { 0.90f, 0.90f, 0.90f }, { 1.00f, 1.00f, 1.00f },
    { 1.00f, 0.00f, 0.00f }, { 0.00f, 1.00f, 0.00f }, { 0.00f, 0.00f, 1.00f },
    { 1.00f, 1.00f, 0.00f }, { 0.00f, 1.00f, 1.00f }, { 1.00f, 0.00f, 1.00f },
    { 0.20f, 0.40f, 0.80f }, { 0.80f, 0.30f, 0.10f }, { 0.90f, 0.75f, 0.55f },
    { 0.30f, 0.05f, 0.08f }, { 0.10f, 0.15f, 0.30f },
};

static void cap_build_core(struct ui_test_glass_build* cap, bool regular, bool light) {
    cap_reset(cap);
    cap_circle(cap, 240.0f);
    cap->cur.regular = regular;
    cap->cur.light = light;
    cap_commit(cap);

    cap_reset(cap);
    cap_rect(cap, 384.0f, 32.0f, false);
    cap->cur.regular = regular;
    cap->cur.light = light;
    cap_commit(cap);

    cap_reset(cap);
    cap_capsule(cap, 320.0f, 80.0f);
    cap->cur.regular = regular;
    cap->cur.light = light;
    cap_commit(cap);

    cap_reset(cap);
    cap_capsule(cap, 448.0f, 48.0f);
    cap->cur.regular = regular;
    cap->cur.light = light;
    cap_commit(cap);
}

static void cap_build_shape(struct ui_test_glass_build* cap) {
    cap->group = "glass-shape";

    for (uint32_t v = 0; v < 4; v++) {
        cap_build_core(cap, v & 1, v >> 1);
    }

    static const float size_ramp[] = { 48.0f, 64.0f, 80.0f, 96.0f, 112.0f, 128.0f, 150.0f, 208.0f, 280.0f, 384.0f, 768.0f };
    for (uint32_t i = 0; i < cr_count(size_ramp); i++) {
        cap_reset(cap);
        cap_circle(cap, size_ramp[i]);
        cap_commit(cap);
        if (i < 9) {
            cap_reset(cap);
            cap_circle(cap, size_ramp[i]);
            cap->cur.regular = 1;
            cap_commit(cap);
        }
    }

    static const struct { float r; bool continuous; } radii[] = {
        { 0.0f, false }, { 8.0f, false }, { 8.0f, true }, { 24.0f, false }, { 24.0f, true },
        { 60.0f, false }, { 60.0f, true }, { 120.0f, false }, { 120.0f, true },
    };
    for (uint32_t i = 0; i < cr_count(radii); i++) {
        cap_reset(cap);
        cap_rect(cap, 240.0f, radii[i].r, radii[i].continuous);
        cap_commit(cap);
    }
    double px = 1.0 / cap->to_px_from_pt;
    cap_reset(cap);
    cap_rect_tag(cap, 384.0 - px, 384.0 - px, 32.0f, false, "re384fr32");
    cap->cur.prims[0].rect.origin.x = floor(cap->cur.prims[0].rect.origin.x) + px;
    cap->cur.prims[0].rect.origin.y = floor(cap->cur.prims[0].rect.origin.y);
    cap_commit(cap);

    static const float full_ladder[] = {
        40.0f, 48.0f, 56.0f, 64.0f, 72.0f, 80.0f, 96.0f, 112.0f, 128.0f, 150.0f,
        176.0f, 208.0f, 240.0f, 280.0f, 320.0f, 384.0f, 480.0f, 640.0f, 768.0f, 1000.0f,
    };
    for (uint32_t v = 0; v < 4; v++) {
        for (uint32_t i = 0; i < cr_count(full_ladder); i++) {
            cap_reset(cap);
            cap_circle(cap, full_ladder[i]);
            cap->cur.regular = v & 1;
            cap->cur.light = v >> 1;
            cap_commit(cap);
        }
    }

    static const float rect_ladder[] = { 48.0f, 64.0f, 96.0f, 128.0f, 150.0f, 208.0f, 240.0f, 320.0f, 384.0f, 480.0f, 640.0f, 768.0f };
    for (uint32_t r = 0; r < 2; r++) {
        for (uint32_t i = 0; i < cr_count(rect_ladder); i++) {
            cap_reset(cap);
            cap_rect(cap, rect_ladder[i], rect_ladder[i] / 8.0f, false);
            cap->cur.regular = r;
            cap_commit(cap);
        }
    }

    static const float grid_mds[] = { 110.0f, 240.0f, 480.0f };
    for (uint32_t r = 0; r < 2; r++) {
        for (uint32_t m = 0; m < cr_count(grid_mds); m++) {
            float md = grid_mds[m];
            const float grid_radii[] = { 0.0f, 4.0f, 8.0f, 16.0f, md / 8.0f, md / 4.0f, md / 2.0f };
            for (uint32_t i = 0; i < 7; i++) {
                cap_reset(cap);
                cap_rect(cap, md, grid_radii[i], false);
                cap->cur.regular = r;
                cap_commit(cap);
            }
        }
    }

    static const float aspects[][2] = {
        { 1000.0f, 160.0f }, { 480.0f, 80.0f }, { 240.0f, 80.0f }, { 640.0f, 120.0f }, { 320.0f, 48.0f }, { 480.0f, 160.0f },
    };
    for (uint32_t r = 0; r < 2; r++) {
        for (uint32_t i = 0; i < cr_count(aspects); i++) {
            cap_reset(cap);
            cap_capsule(cap, aspects[i][0], aspects[i][1]);
            cap->cur.regular = r;
            cap_commit(cap);
        }
    }
    cap_reset(cap);
    cap_rect_tag(cap, 240.0f, 240.0f, 32.0f, false, "re240fo50");
    cap->cur.prims[0].rect.origin.x = floor(cap->cur.prims[0].rect.origin.x) + px;
    cap->cur.prims[0].rect.origin.y = floor(cap->cur.prims[0].rect.origin.y) + px;
    cap_commit(cap);
}

static void cap_build_slider(struct ui_test_glass_build* cap) {
    cap->group = "glass-slider";

    static const float slider_stops[] = { 0.0f, 0.25f, 0.75f, 1.0f };
    for (uint32_t r = 0; r < 2; r++) {
        for (uint32_t s = 0; s < cr_count(slider_stops); s++) {
            cap_reset(cap);
            cap_circle(cap, 240.0f);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap_commit(cap);

            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap_commit(cap);
        }
    }

    static const float slider_bgs[] = { 0.0f, 0.18f, 0.65f, 1.0f };
    for (uint32_t r = 0; r < 2; r++) {
        for (uint32_t s = 0; s < cr_count(slider_stops); s++) {
            for (uint32_t b = 0; b < cr_count(slider_bgs); b++) {
                cap_reset(cap);
                cap_circle(cap, 240.0f);
                cap->cur.regular = r;
                cap->cur.slider = slider_stops[s];
                cap_bg_gray(cap, slider_bgs[b]);
                cap_commit(cap);
            }

            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap_tint(cap, 0.0f, 0.0f, 1.0f, 0.5f);
            cap_commit(cap);

            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap_tint(cap, 1.0f, 1.0f, 1.0f, 1.0f);
            cap_commit(cap);

            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap_tint(cap, 0.5f, 0.5f, 0.5f, 0.5f);
            cap_commit(cap);

            cap_reset(cap);
            cap_circle(cap, 240.0f);
            cap->cur.regular = r;
            cap->cur.slider = slider_stops[s];
            cap->cur.pattern = 10;
            cap_commit(cap);

            static const int16_t slider_patterns[] = { 5, 15, 17, 19 };
            for (uint32_t p = 0; p < cr_count(slider_patterns); p++) {
                cap_reset(cap);
                cap_rect(cap, 384.0f, 32.0f, false);
                cap->cur.regular = r;
                cap->cur.slider = slider_stops[s];
                cap->cur.pattern = slider_patterns[p];
                cap_commit(cap);
            }
        }
    }
}

static void cap_build_tint(struct ui_test_glass_build* cap) {
    cap->group = "glass-tint";

    static const float tint_alphas[] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f };
    for (uint32_t i = 0; i < cr_count(tint_alphas); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap_tint(cap, 0.0f, 0.0f, 1.0f, tint_alphas[i]);
        cap_commit(cap);
    }
    static const float tint_grays[] = { 0.15f, 0.5f, 0.75f, 1.0f };
    for (uint32_t i = 0; i < cr_count(tint_grays); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap_tint(cap, tint_grays[i], tint_grays[i], tint_grays[i], 1.0f);
        cap_commit(cap);
    }
    static const float tint_hues[][3] = {
        { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, { 0.8f, 0.3f, 0.1f },
    };
    for (uint32_t i = 0; i < cr_count(tint_hues); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap_tint(cap, tint_hues[i][0], tint_hues[i][1], tint_hues[i][2], 1.0f);
        cap_commit(cap);
    }
    cap_reset(cap);
    cap_rect(cap, 384.0f, 32.0f, false);
    cap_tint(cap, 1.0f, 1.0f, 1.0f, 1.0f);
    cap_commit(cap);
    cap_reset(cap);
    cap_rect(cap, 384.0f, 32.0f, false);
    cap_tint(cap, 0.0f, 0.0f, 0.0f, 1.0f);
    cap_commit(cap);

    static const float rg_tints[][4] = {
        { 0.0f, 0.0f, 1.0f, 0.5f }, { 0.5f, 0.5f, 0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    for (uint32_t i = 0; i < cr_count(rg_tints); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap->cur.regular = 1;
        cap_tint(cap, rg_tints[i][0], rg_tints[i][1], rg_tints[i][2], rg_tints[i][3]);
        cap_commit(cap);
    }
    cap_reset(cap);
    cap_rect(cap, 384.0f, 32.0f, false);
    cap->cur.light = 1;
    cap_tint(cap, 0.0f, 0.0f, 1.0f, 0.5f);
    cap_commit(cap);
    cap_reset(cap);
    cap_rect(cap, 384.0f, 32.0f, false);
    cap->cur.regular = 1;
    cap->cur.light = 1;
    cap_tint(cap, 0.5f, 0.5f, 0.5f, 1.0f);
    cap_commit(cap);

    for (uint32_t t = 0; t < cr_count(cap_color_sweep_); t++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap_tint(cap, cap_color_sweep_[t][0], cap_color_sweep_[t][1], cap_color_sweep_[t][2], 1.0f);
        cap_commit(cap);
        if (t < 15) {
            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = 1;
            cap_tint(cap, cap_color_sweep_[t][0], cap_color_sweep_[t][1], cap_color_sweep_[t][2], 1.0f);
            cap_commit(cap);
        }
    }
    static const float alpha_tints[][3] = {
        { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.8f, 0.3f, 0.1f },
    };
    static const float alphas[] = { 0.125f, 0.25f, 0.5f, 0.75f };
    for (uint32_t t = 0; t < cr_count(alpha_tints); t++) {
        for (uint32_t a = 0; a < cr_count(alphas); a++) {
            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap_tint(cap, alpha_tints[t][0], alpha_tints[t][1], alpha_tints[t][2], alphas[a]);
            cap_commit(cap);
        }
    }
    for (uint32_t b = 0; b < cr_count(cap_color_sweep_); b++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        if (b < 11) {
            cap_bg_gray(cap, cap_color_sweep_[b][0]);
        } else {
            cap_bg_rgb(cap, cap_color_sweep_[b][0], cap_color_sweep_[b][1], cap_color_sweep_[b][2]);
        }
        cap_tint(cap, 0.0f, 0.0f, 1.0f, 0.75f);
        cap_commit(cap);
    }
    static const float gray_tints[] = { 0.15f, 0.5f, 0.75f, 1.0f };
    static const uint32_t gray_bgs[] = { 0, 3, 5, 7, 9, 10 };
    for (uint32_t t = 0; t < cr_count(gray_tints); t++) {
        for (uint32_t b = 0; b < cr_count(gray_bgs); b++) {
            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap_bg_gray(cap, cap_color_sweep_[gray_bgs[b]][0]);
            cap_tint(cap, gray_tints[t], gray_tints[t], gray_tints[t], 1.0f);
            cap_commit(cap);
        }
    }
    static const float ladder_tints[][3] = {
        { 0.52f, 0.50f, 0.48f }, { 0.55f, 0.50f, 0.45f }, { 0.60f, 0.50f, 0.40f },
        { 0.70f, 0.50f, 0.30f }, { 0.90f, 0.50f, 0.10f }, { 0.30f, 0.28f, 0.26f },
        { 0.34f, 0.28f, 0.22f }, { 0.80f, 0.78f, 0.76f }, { 0.84f, 0.78f, 0.72f },
    };
    static const float ladder_bgs[] = { 0.18f, 0.65f };
    for (uint32_t t = 0; t < cr_count(ladder_tints); t++) {
        for (uint32_t b = 0; b < cr_count(ladder_bgs); b++) {
            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap_bg_gray(cap, ladder_bgs[b]);
            cap_tint(cap, ladder_tints[t][0], ladder_tints[t][1], ladder_tints[t][2], 1.0f);
            cap_commit(cap);
        }
    }
    static const uint32_t lt_hue_tints[] = { 11, 12, 13, 14, 15, 16, 18, 21 };
    for (uint32_t i = 0; i < cr_count(lt_hue_tints); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap->cur.light = 1;
        cap_tint(cap, cap_color_sweep_[lt_hue_tints[i]][0], cap_color_sweep_[lt_hue_tints[i]][1], cap_color_sweep_[lt_hue_tints[i]][2], 1.0f);
        cap_commit(cap);
    }
}

static void cap_build_bg(struct ui_test_glass_build* cap) {
    cap->group = "glass-bg";

    for (uint32_t i = 0; i < 11; i++) {
        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap_bg_gray(cap, cap_color_sweep_[i][0]);
        cap_commit(cap);
    }
    static const uint32_t bg_colors[] = { 11, 12, 13, 18, 21 };
    for (uint32_t i = 0; i < 5; i++) {
        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap_bg_rgb(cap, cap_color_sweep_[bg_colors[i]][0], cap_color_sweep_[bg_colors[i]][1], cap_color_sweep_[bg_colors[i]][2]);
        cap_commit(cap);
    }
    static const uint32_t rg_bg_grays[] = { 0, 3, 5, 7 };
    for (uint32_t i = 0; i < cr_count(rg_bg_grays); i++) {
        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.regular = 1;
        cap_bg_gray(cap, cap_color_sweep_[rg_bg_grays[i]][0]);
        cap_commit(cap);
    }

    for (uint32_t v = 1; v < 4; v++) {
        for (uint32_t b = 0; b < 11; b++) {
            cap_reset(cap);
            cap_circle(cap, 240.0f);
            cap->cur.regular = v & 1;
            cap->cur.light = v >> 1;
            cap_bg_gray(cap, cap_color_sweep_[b][0]);
            cap_commit(cap);
        }
    }
    for (uint32_t i = 0; i < cr_count(bg_colors); i++) {
        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.regular = 1;
        cap_bg_rgb(cap, cap_color_sweep_[bg_colors[i]][0], cap_color_sweep_[bg_colors[i]][1], cap_color_sweep_[bg_colors[i]][2]);
        cap_commit(cap);

        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.light = 1;
        cap_bg_rgb(cap, cap_color_sweep_[bg_colors[i]][0], cap_color_sweep_[bg_colors[i]][1], cap_color_sweep_[bg_colors[i]][2]);
        cap_commit(cap);
    }
    for (uint32_t v = 0; v < 4; v++) {
        for (uint32_t b = 0; b < 11; b++) {
            cap_reset(cap);
            cap_rect(cap, 64.0f, 20.0f, false);
            cap->cur.regular = v & 1;
            cap->cur.light = v >> 1;
            cap_bg_gray(cap, cap_color_sweep_[b][0]);
            cap_commit(cap);

            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = v & 1;
            cap->cur.light = v >> 1;
            cap_bg_gray(cap, cap_color_sweep_[b][0]);
            cap_commit(cap);
        }
    }
    for (uint32_t i = 0; i < cr_count(bg_colors); i++) {
        for (uint32_t r = 0; r < 2; r++) {
            cap_reset(cap);
            cap_rect(cap, 64.0f, 20.0f, false);
            cap->cur.regular = r;
            cap_bg_rgb(cap, cap_color_sweep_[bg_colors[i]][0], cap_color_sweep_[bg_colors[i]][1], cap_color_sweep_[bg_colors[i]][2]);
            cap_commit(cap);
        }
    }
}

static void cap_build_shadow(struct ui_test_glass_build* cap) {
    cap->group = "glass-shadow";

    static const float shadow_sizes[] = { 48.0f, 96.0f, 150.0f, 240.0f, 384.0f, 768.0f };
    for (uint32_t i = 0; i < cr_count(shadow_sizes); i++) {
        for (uint32_t bg = 0; bg < 2; bg++) {
            cap_reset(cap);
            cap_circle(cap, shadow_sizes[i]);
            cap_bg_gray(cap, bg == 0 ? 0.0f : 1.0f);
            cap_commit(cap);
            if (shadow_sizes[i] == 96.0f || shadow_sizes[i] == 240.0f || shadow_sizes[i] == 768.0f) {
                cap_reset(cap);
                cap_circle(cap, shadow_sizes[i]);
                cap->cur.regular = 1;
                cap_bg_gray(cap, bg == 0 ? 0.0f : 1.0f);
                cap_commit(cap);
            }
        }
    }
}

static void cap_build_pattern(struct ui_test_glass_build* cap) {
    cap->group = "glass-pattern";

    static const int16_t std_patterns[] = { 1, 5, 7, 10, 11, 13, 14, 15, 17, 28 };
    for (uint32_t i = 0; i < cr_count(std_patterns); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap->cur.pattern = std_patterns[i];
        cap_commit(cap);

        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.pattern = std_patterns[i];
        cap_commit(cap);
    }

    for (int16_t pattern = 31; pattern <= 34; pattern++) {
        for (uint32_t r = 0; r < 2; r++) {
            cap_reset(cap);
            cap_rect(cap, 384.0f, 32.0f, false);
            cap->cur.regular = r;
            cap->cur.pattern = pattern;
            cap_commit(cap);

            cap_reset(cap);
            cap_circle(cap, 240.0f);
            cap->cur.regular = r;
            cap->cur.pattern = pattern;
            cap_commit(cap);
        }
    }

    for (int16_t pattern = 1; pattern <= 30; pattern++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap->cur.pattern = pattern;
        cap_commit(cap);

        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.pattern = pattern;
        cap_commit(cap);
    }
    static const float deep_sizes[] = { 64.0f, 150.0f, 240.0f, 280.0f };
    for (int16_t pattern = 20; pattern <= 34; pattern++) {
        for (uint32_t i = 0; i < cr_count(deep_sizes); i++) {
            cap_reset(cap);
            cap_circle(cap, deep_sizes[i]);
            cap->cur.pattern = pattern;
            cap_commit(cap);
        }
    }
    static const int16_t rg_patterns[] = { 10, 15, 17, 28 };
    for (uint32_t i = 0; i < cr_count(rg_patterns); i++) {
        cap_reset(cap);
        cap_rect(cap, 384.0f, 32.0f, false);
        cap->cur.regular = 1;
        cap->cur.pattern = rg_patterns[i];
        cap_commit(cap);

        cap_reset(cap);
        cap_circle(cap, 240.0f);
        cap->cur.regular = 1;
        cap->cur.pattern = rg_patterns[i];
        cap_commit(cap);
    }
}

static void cap_build_union_case(struct ui_test_glass_build* cap, uint32_t which, bool regular) {
    cap_reset(cap);
    cap->cur.regular = regular;
    switch (which) {
    case 0: cap_union_pair_circles(cap, 150.0f, -24.0f); break;
    case 1: cap_union_pair_circles(cap, 150.0f, 0.0f); break;
    case 2: cap_union_pair_circles(cap, 150.0f, 16.0f); break;
    case 3: cap_union_pair_circles(cap, 150.0f, 40.0f); break;
    case 4: {
        cap->cur.canvas_w = 704;
        cap->cur.canvas_h = 512;
        cap->cur.md = 240.0f;
        cap->cur.prim_count = 2;
        cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { 264.0, 256.0 }, .radius = 120.0 } };
        cap->cur.prims[1] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { 439.0, 256.0 }, .radius = 55.0 } };
        cap_union_named(cap, "u-cc240x110", "g0");
        break;
    }
    case 5: {
        cap->cur.canvas_w = 704;
        cap->cur.canvas_h = 512;
        cap->cur.md = 150.0f;
        cap->cur.prim_count = 2;
        cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { 252.0, 256.0 }, .radius = 75.0 } };
        cap->cur.prims[1] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_rect, .rect = { .origin = { 327.0, 181.0 }, .width = 150.0, .height = 150.0, .radius = 24.0 } };
        cap_union_named(cap, "u-cre150", "g0");
        break;
    }
    default: {
        cap->cur.canvas_w = 832;
        cap->cur.canvas_h = 384;
        cap->cur.md = 60.0f;
        cap->cur.prim_count = 3;
        for (uint32_t i = 0; i < 3; i++) {
            cap->cur.prims[i] = (struct ui_sdf_prim) {
                .type = ui_sdf_prim_type_rect,
                .rect = { .origin = { 148.0 + i * 168.0, 162.0 }, .width = 180.0, .height = 60.0, .radius = 30.0 },
            };
        }
        cap_union_named(cap, "u-3cap", "g-12");
        break;
    }
    }
    cap_commit(cap);
}

static void cap_build_union(struct ui_test_glass_build* cap) {
    cap->group = "glass-union";

    for (uint32_t which = 0; which < 7; which++) {
        cap_build_union_case(cap, which, false);
    }
    cap_build_union_case(cap, 1, true);
    cap_build_union_case(cap, 2, true);
    cap_build_union_case(cap, 4, true);
    cap_build_union_case(cap, 5, true);
    cap_build_union_case(cap, 6, true);

    static const float sibling_gaps[] = { 0.0f, 16.0f, 40.0f };
    for (uint32_t i = 0; i < cr_count(sibling_gaps); i++) {
        cap_reset(cap);
        cap_union_pair_circles(cap, 150.0f, sibling_gaps[i]);
        cap_union_siblings(cap);
        cap_commit(cap);
    }
    cap_reset(cap);
    cap_union_pair_circles(cap, 150.0f, 16.0f);
    cap_union_siblings(cap);
    cap->cur.regular = 1;
    cap_commit(cap);

    static const float union_gaps[] = { -48.0f, -24.0f, -8.0f, 0.0f, 4.0f, 8.0f, 16.0f, 24.0f, 40.0f, 64.0f };
    for (uint32_t i = 0; i < cr_count(union_gaps); i++) {
        cap_reset(cap);
        cap_union_pair_circles(cap, 150.0f, union_gaps[i]);
        cap_commit(cap);
        cap_reset(cap);
        cap_union_pair_circles(cap, 150.0f, union_gaps[i]);
        cap_union_siblings(cap);
        cap_commit(cap);
        if (union_gaps[i] == -8.0f || union_gaps[i] == 0.0f || union_gaps[i] == 16.0f) {
            cap_reset(cap);
            cap_union_pair_circles(cap, 150.0f, union_gaps[i]);
            cap->cur.regular = 1;
            cap_commit(cap);
        }
        cap_reset(cap);
        cap_union_pair_circles(cap, 64.0f, union_gaps[i]);
        cap_commit(cap);
        cap_reset(cap);
        cap_union_pair_circles(cap, 280.0f, union_gaps[i]);
        cap_commit(cap);
    }
}

static void cap_build_edge(struct ui_test_glass_build* cap) {
    cap->group = "glass-edge";

    for (uint32_t which = 0; which < 4; which++) {
        for (uint32_t r = 0; r < 2; r++) {
            cap_reset(cap);
            cap->cur.regular = r;
            cap->cur.canvas_w = 512;
            cap->cur.canvas_h = 512;
            cap->cur.prim_count = 1;
            switch (which) {
            case 0:
                cap->cur.md = 240.0f;
                cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { 40.0, 256.0 }, .radius = 120.0 } };
                snprintf(cap->cur.shape, sizeof(cap->cur.shape), "cx-left240");
                break;
            case 1:
                cap->cur.md = 240.0f;
                cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_circle, .circle = { .center = { 16.0, 16.0 }, .radius = 120.0 } };
                snprintf(cap->cur.shape, sizeof(cap->cur.shape), "cx-corner240");
                break;
            case 2:
                cap->cur.md = 80.0f;
                cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_rect, .rect = { .origin = { 96.0, -16.0 }, .width = 320.0, .height = 80.0, .radius = 40.0 } };
                snprintf(cap->cur.shape, sizeof(cap->cur.shape), "cx-top320x80");
                break;
            default:
                cap->cur.md = 240.0f;
                cap->cur.prims[0] = (struct ui_sdf_prim) { .type = ui_sdf_prim_type_rect, .rect = { .origin = { 380.0, 136.0 }, .width = 240.0, .height = 240.0, .radius = 48.0 } };
                snprintf(cap->cur.shape, sizeof(cap->cur.shape), "cx-right240");
                break;
            }
            cap_commit(cap);
        }
    }
}

static void cap_build_edr(struct ui_test_glass_build* cap) {
    cap->group = "glass-edr";

    cap_reset(cap);
    cap->cur.kind = ui_test_capture_kind_edr_ramp;
    cap->cur.canvas_w = 512;
    cap->cur.canvas_h = 128;
    cap->cur.background = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
    cap->cur.edr_boost = 2.0f;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "edr-ramp");
    snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "edrramp");
    cap_commit_named(cap, "edr-ramp");

    static const char* edr_styles[] = { "edrpatch", "edrstripe", "edrrim" };
    for (uint32_t style = 0; style < cr_count(edr_styles); style++) {
        for (uint32_t r = 0; r < 2; r++) {
            cap_reset(cap);
            cap_circle(cap, 240.0f);
            cap->cur.regular = r;
            cap->cur.edr_boost = 2.0f;
            snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "%s", edr_styles[style]);
            cap_commit(cap);
        }
    }

    static const float edr_boosts[] = { 1.5f, 2.0f };
    for (uint32_t style = 0; style < cr_count(edr_styles); style++) {
        for (uint32_t b = 0; b < cr_count(edr_boosts); b++) {
            for (uint32_t r = 0; r < 2; r++) {
                cap_reset(cap);
                cap_circle(cap, 240.0f);
                cap->cur.regular = r;
                cap->cur.edr_boost = edr_boosts[b];
                snprintf(cap->cur.bg_tag, sizeof(cap->cur.bg_tag), "%s", edr_styles[style]);
                cap_commit(cap);
            }
        }
    }
}

static void cap_perf_row_circles(struct ui_test_glass_build* cap, uint32_t count, float md, float gap) {
    assert(count <= cr_count(cap->cur.prims));
    float span = (float)count * md + (float)(count - 1) * gap;
    cap->cur.canvas_w = cap_canvas_dim(span);
    cap->cur.canvas_h = cap_canvas_dim(md);
    cap->cur.md = md;
    cap->cur.prim_count = (uint8_t)count;
    double y = cap->cur.canvas_h * 0.5;
    double x = (cap->cur.canvas_w - span) * 0.5 + md * 0.5;
    for (uint32_t i = 0; i < count; i++) {
        cap->cur.prims[i] = (struct ui_sdf_prim) {
            .type = ui_sdf_prim_type_circle,
            .circle = { .center = { x + (double)i * (double)(md + gap), y }, .radius = md * 0.5 },
        };
        cap->cur.union_groups[i] = i;
    }
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "gp-row%uc%.0fs", count, (double)md);
}

static void cap_perf_tri_circles(struct ui_test_glass_build* cap, float md, float gap, bool siblings) {
    float span = 3.0f * md + 2.0f * gap;
    cap->cur.canvas_w = cap_canvas_dim(span);
    cap->cur.canvas_h = cap_canvas_dim(md);
    cap->cur.md = md;
    cap->cur.prim_count = 3;
    double y = cap->cur.canvas_h * 0.5;
    double x = (cap->cur.canvas_w - span) * 0.5 + md * 0.5;
    for (uint32_t i = 0; i < 3; i++) {
        cap->cur.prims[i] = (struct ui_sdf_prim) {
            .type = ui_sdf_prim_type_circle,
            .circle = { .center = { x + (double)i * (double)(md + gap), y }, .radius = md * 0.5 },
        };
        cap->cur.union_groups[i] = siblings ? i : 0;
    }
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "gp-tri%.0f%s", (double)md, siblings ? "s" : "");
}

static void cap_perf_toolbar(struct ui_test_glass_build* cap) {
    cap->cur.canvas_w = 896;
    cap->cur.canvas_h = 448;
    cap->cur.md = 64.0f;
    cap->cur.prim_count = 5;
    cap->cur.prims[0] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_rect,
        .rect = { .origin = { 128.0, 228.0 }, .width = 640.0, .height = 64.0, .radius = 32.0 },
    };
    for (uint32_t i = 0; i < 4; i++) {
        cap->cur.prims[1 + i] = (struct ui_sdf_prim) {
            .type = ui_sdf_prim_type_circle,
            .circle = { .center = { 340.0 + (double)i * 72.0, 180.0 }, .radius = 24.0 },
        };
    }
    for (uint32_t i = 0; i < 5; i++) {
        cap->cur.union_groups[i] = i;
    }
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "gp-bar640b4s");
}

static void cap_perf_sheet(struct ui_test_glass_build* cap) {
    cap->cur.canvas_w = 1032;
    cap->cur.canvas_h = 896;
    cap->cur.md = 640.0f;
    cap->cur.radius = 32.0f;
    cap->cur.prim_count = 1;
    cap->cur.prims[0] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_rect,
        .rect = { .origin = { 16.0, 128.0 }, .width = 1000.0, .height = 640.0, .radius = 32.0 },
    };
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "gp-sheet1000");
}

static void cap_perf_mixed(struct ui_test_glass_build* cap) {
    cap->cur.canvas_w = 1032;
    cap->cur.canvas_h = 704;
    cap->cur.md = 240.0f;
    cap->cur.prim_count = 3;
    cap->cur.prims[0] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_rect,
        .rect = { .origin = { 48.0, 192.0 }, .width = 480.0, .height = 320.0, .radius = 48.0 },
    };
    cap->cur.prims[1] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_circle,
        .circle = { .center = { 768.0, 240.0 }, .radius = 120.0 },
    };
    cap->cur.prims[2] = (struct ui_sdf_prim) {
        .type = ui_sdf_prim_type_rect,
        .rect = { .origin = { 600.0, 500.0 }, .width = 320.0, .height = 80.0, .radius = 40.0 },
    };
    for (uint32_t i = 0; i < 3; i++) {
        cap->cur.union_groups[i] = i;
    }
    cap->cur.use_union = true;
    snprintf(cap->cur.shape, sizeof(cap->cur.shape), "gp-mix3s");
}

static void cap_build_perf(struct ui_test_glass_build* cap) {
    cap->group = "glass-perf";

    static const struct { uint32_t count; float md; } rows[] = {
        { 4, 64.0f }, { 8, 64.0f }, { 4, 150.0f },
    };
    for (uint32_t i = 0; i < cr_count(rows); i++) {
        for (uint32_t r = 0; r < 2; r++) {
            cap_reset(cap);
            cap_perf_row_circles(cap, rows[i].count, rows[i].md, 32.0f);
            cap->cur.regular = r;
            cap_commit(cap);
        }
    }

    cap_reset(cap);
    cap_perf_tri_circles(cap, 150.0f, 16.0f, false);
    cap_commit(cap);
    cap_reset(cap);
    cap_perf_tri_circles(cap, 150.0f, 16.0f, true);
    cap_commit(cap);
    cap_reset(cap);
    cap_perf_tri_circles(cap, 150.0f, 16.0f, false);
    cap->cur.regular = 1;
    cap_commit(cap);

    for (uint32_t r = 0; r < 2; r++) {
        cap_reset(cap);
        cap_perf_toolbar(cap);
        cap->cur.regular = r;
        cap_commit(cap);

        cap_reset(cap);
        cap_perf_sheet(cap);
        cap->cur.regular = r;
        cap_commit(cap);

        cap_reset(cap);
        cap_perf_mixed(cap);
        cap->cur.regular = r;
        cap_commit(cap);
    }

    cap_reset(cap);
    cap_perf_row_circles(cap, 8, 64.0f, 32.0f);
    cap_tint(cap, 0.0f, 0.0f, 1.0f, 0.5f);
    cap_commit(cap);

    for (uint32_t r = 0; r < 2; r++) {
        cap_reset(cap);
        cap_perf_sheet(cap);
        cap->cur.regular = r;
        cap_tint(cap, 0.0f, 0.0f, 1.0f, 0.5f);
        cap_commit(cap);
    }
}

static void cap_build(struct ui_test_glass_build* cap) {
    cap_build_shape(cap);
    cap_build_slider(cap);
    cap_build_tint(cap);
    cap_build_bg(cap);
    cap_build_shadow(cap);
    cap_build_pattern(cap);
    cap_build_union(cap);
    cap_build_edge(cap);
    cap_build_edr(cap);
    cap_build_perf(cap);
}

static struct ui_node ui_test_glass_scene_build(struct ui_test_scene_state* state, struct ui_node parent, double2 size, const struct ui_test_scene* scene) {
    const struct ui_test_capture_scene* s = scene->cap;

    float4 base = s->background;
    struct ui_test_glass_pattern_rect rects[ui_test_glass_rfr_max_rects];
    uint32_t rect_count = 0;
    if (s->pattern >= 0) {
        rect_count = ui_test_glass_pattern_rects((uint32_t)s->pattern, make_double2(s->canvas_w, s->canvas_h), ui_test_glass_shape_left(s), scene->to_px_from_pt, &base, rects, cr_count(rects));
    }

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .parent = parent,
        .size = ui_size_fix(size.x, size.y),
        .fill = ui_fill_solid(ui_rgba(base.x, base.y, base.z, 1.0f)),
        .clip = true,
    });

    for (uint32_t i = 0; i < rect_count; i++) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(rects[i].origin.x, rects[i].origin.y),
            .size = ui_size_fix(rects[i].size.x, rects[i].size.y),
            .fill = ui_fill_solid(ui_rgba(rects[i].color.x, rects[i].color.y, rects[i].color.z, rects[i].color.w)),
        });
    }

    if (s->edr_boost > 0.0f) {
        struct ui_test_glass_pattern_rect edr_rects[64];
        uint32_t edr_count = ui_test_glass_edr_rects(s, scene->to_px_from_pt, edr_rects, cr_count(edr_rects));
        for (uint32_t i = 0; i < edr_count; i++) {
            ui_frame(&(struct ui_frame) {
                .parent = root,
                .pos = ui_pos_xy(edr_rects[i].origin.x, edr_rects[i].origin.y),
                .size = ui_size_fix(edr_rects[i].size.x, edr_rects[i].size.y),
                .fill = ui_fill_solid(ui_color(edr_rects[i].color, ui_color_space_srgb_linear)),
            });
        }
    }

    if (s->prim_count == 0) {
        return root;
    }

    struct ui_color tint_color = s->tint.w > 0.0f ? ui_rgba(s->tint.x, s->tint.y, s->tint.z, s->tint.w) : (struct ui_color) { 0 };

    uint32_t group_ids[8] = { 0 };
    uint32_t group_count = 1;
    if (s->use_union) {
        group_count = 0;
        for (uint32_t i = 0; i < s->prim_count; i++) {
            bool seen = false;
            for (uint32_t g = 0; g < group_count; g++) {
                seen = seen || group_ids[g] == s->union_groups[i];
            }
            if (seen == false) {
                group_ids[group_count++] = s->union_groups[i];
            }
        }
    }

    for (uint32_t g = 0; g < group_count; g++) {
        struct ui_node group = ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(0.0f, 0.0f),
            .size = ui_size_fix(size.x, size.y),
            .shape_group = { .op = ui_shape_group_op_union },
            .fill.layers[0] = ui_layer_glass_ex((enum ui_glass_variant)s->regular, (enum ui_glass_appearance)s->light, tint_color, s->slider),
        });
        for (uint32_t i = 0; i < s->prim_count; i++) {
            if (s->use_union && s->union_groups[i] != group_ids[g]) {
                continue;
            }
            if (s->prims[i].type == ui_sdf_prim_type_circle) {
                double radius = s->prims[i].circle.radius;
                ui_ellipse(&(struct ui_ellipse) {
                    .parent = group,
                    .pos = ui_pos_xy(s->prims[i].circle.center.x - radius, s->prims[i].circle.center.y - radius),
                    .size = ui_size_fix(2.0 * radius, 2.0 * radius),
                });
            } else {
                ui_rect(&(struct ui_rect) {
                    .parent = group,
                    .pos = ui_pos_xy(s->prims[i].rect.origin.x, s->prims[i].rect.origin.y),
                    .size = ui_size_fix(s->prims[i].rect.width, s->prims[i].rect.height),
                    .corners = {
                        .profile = s->prims[i].rect.continuous ? ui_corner_profile_continuous : ui_corner_profile_circular,
                        .radius = ui_pad(s->prims[i].rect.radius),
                    },
                });
            }
        }
    }

    return root;
}

static void ui_test_glass_scene_thresholds(const struct ui_test_capture_scene* s, struct ui_test_scene* out) {
    out->mean_threshold = 0.008f;
    out->tile_threshold = 0.075f;
    out->alarm_allowance = 2500;
    if (s->tint.w > 0.0f || s->regular) {
        out->mean_threshold = 0.004f;
        out->tile_threshold = 0.09f;
    }
    if (s->kind != ui_test_capture_kind_glass || s->edr_boost > 0.0f) {
        out->mean_threshold = 0.02f;
        out->tile_threshold = 0.08f;
        return;
    }
    if (s->prim_count == 1 && s->pattern < 0) {
        if (s->prims[0].type == ui_sdf_prim_type_circle) {
            out->band_shape = (struct ui_test_scene_band_shape) {
                .type = ui_test_scene_band_shape_circle,
                .origin = { (float)s->prims[0].circle.center.x, (float)s->prims[0].circle.center.y },
                .radius = (float)s->prims[0].circle.radius,
            };
        } else {
            out->band_shape = (struct ui_test_scene_band_shape) {
                .type = ui_test_scene_band_shape_rect,
                .origin = { (float)s->prims[0].rect.origin.x, (float)s->prims[0].rect.origin.y },
                .size = { (float)s->prims[0].rect.width, (float)s->prims[0].rect.height },
                .radius = (float)s->prims[0].rect.radius,
            };
        }
        out->rim_threshold = 0.035f;
        out->bevel_threshold = 0.02f;
        out->interior_threshold = 0.024f;
    }
}

void ui_test_scenes_glass_build_(struct ui_test_scenes* scenes) {
    struct ui_test_capture_scene* captures = cr_malloc(scenes->mem, ui_test_capture_max * sizeof(*captures), 16, cr_loc);
    struct ui_test_scene* glass_scenes = cr_malloc(scenes->mem, ui_test_capture_max * sizeof(*glass_scenes), 16, cr_loc);
    memset(captures, 0, ui_test_capture_max * sizeof(*captures));
    memset(glass_scenes, 0, ui_test_capture_max * sizeof(*glass_scenes));

    struct ui_test_glass_build build = { .scenes = captures, .to_px_from_pt = scenes->desc.screen_to_px_from_pt };
    cap_build(&build);
    assert(build.count > 0);

    for (uint32_t i = 0; i < build.count; i++) {
        const struct ui_test_capture_scene* s = &captures[i];
        struct ui_test_scene* scene = &glass_scenes[i];
        scene->label = s->name;
        scene->group = s->group;
        scene->size = make_double2(s->canvas_w, s->canvas_h);
        scene->capture = ui_test_scene_capture_screenshot;
        scene->cap = s;
        scene->build = ui_test_glass_scene_build;
        ui_test_glass_scene_thresholds(s, scene);
    }

    scenes->glass_captures = captures;
    scenes->glass_scenes = glass_scenes;
    scenes->glass_count = build.count;
}

const struct ui_test_scene* ui_test_scenes_glass_(struct ui_test_scenes* scenes, uint32_t* out_count) {
    *out_count = scenes->glass_count;
    return scenes->glass_scenes;
}

#if cr_ios
void ui_test_glass_scene_present_(const struct ui_test_scene* scene) {
    const struct ui_test_capture_scene* s = scene->cap;
    struct ui_test_glass_pattern_rect rects[ui_test_glass_rfr_max_rects];
    struct ui_test_glass_pattern_rect edr_rects[64];
    float4 background = s->background;
    uint32_t rect_count = 0;
    uint32_t edr_count = 0;
    if (s->pattern >= 0) {
        float4 base;
        rect_count = ui_test_glass_pattern_rects((uint32_t)s->pattern, make_double2(s->canvas_w, s->canvas_h), ui_test_glass_shape_left(s), scene->to_px_from_pt, &base, rects, cr_count(rects));
        background = base;
    }
    if (s->edr_boost > 0.0f) {
        edr_count = ui_test_glass_edr_rects(s, scene->to_px_from_pt, edr_rects, cr_count(edr_rects));
    }
    [SwiftUIShapesBridge renderWithWidth:scene->width height:scene->height
                                   prims:s->prim_count > 0 ? s->prims : NULL primCount:s->prim_count
                              background:(const float*)&background tint:(const float*)&s->tint
                              primUnions:s->use_union ? s->union_groups : NULL
                                 variant:s->regular appearance:s->light
                            patternRects:rect_count > 0 ? rects : NULL patternRectCount:rect_count
                                edrRects:edr_count > 0 ? edr_rects : NULL edrRectCount:edr_count
                              toPxFromPt:scene->to_px_from_pt];
}
#endif
