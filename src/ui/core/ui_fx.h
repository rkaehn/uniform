#pragma once

#include "ui.h"
#include <math.h>

#define ui_fx_zero ((struct ui_fx) { .is_init = true, ._raw = 0 })
#define ui_fx_range_min (-((int32_t)1 << 24))
#define ui_fx_range_max ((int32_t)1 << 24)
#define ui_fx_fbits 6

struct ui_fx {
    bool is_init;
    int32_t _raw;
};

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_make(int32_t x) {
    assert(x >= ui_fx_range_min);
    assert(x <= ui_fx_range_max);
    struct ui_fx result;
    result.is_init = true;
    result._raw = x << ui_fx_fbits;
    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_make_round(double x) {
    double scaled = x * (double)(1 << ui_fx_fbits);
    double resultf = round(scaled);
    assert((resultf / (double)(1 << ui_fx_fbits)) >= ui_fx_range_min);
    assert((resultf / (double)(1 << ui_fx_fbits)) <= ui_fx_range_max);
    struct ui_fx result;
    result.is_init = true;
    result._raw = (int32_t)resultf;
    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_make_px(double pt, double to_px_from_pt) {
    return ui_fx_make((int32_t)lround(pt * to_px_from_pt));
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_make_px_size(double pt, double to_px_from_pt) {
    int32_t px = (int32_t)lround(pt * to_px_from_pt);
    if (px == 0 && pt > 0.0) {
        px = 1;
    }
    return ui_fx_make(px);
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_round_px(struct ui_fx x) {
    assert(x.is_init);
    struct ui_fx result;
    result.is_init = true;
    result._raw = ((x._raw + 32) >> ui_fx_fbits) << ui_fx_fbits;
    return result;
}

cr_hdef __attribute__((always_inline))
bool ui_fx_is_whole_px(struct ui_fx x) {
    assert(x.is_init);
    return (x._raw & ((1 << ui_fx_fbits) - 1)) == 0;
}

cr_hdef __attribute__((always_inline))
double ui_fx_to_double(struct ui_fx x) {
    assert(x.is_init);
    return (double)x._raw / (double)(1u << ui_fx_fbits);
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_add(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    struct ui_fx result;
    result.is_init = true;
    int error = __builtin_add_overflow(a._raw, b._raw, &result._raw);
    assert(error == 0);
    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_sub(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    struct ui_fx result;
    result.is_init = true;
    int error = __builtin_sub_overflow(a._raw, b._raw, &result._raw);
    assert(error == 0);
    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_mul(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    struct ui_fx result;
    result.is_init = true;

    int64_t prod = (int64_t)a._raw * (int64_t)b._raw;
    prod >>= ui_fx_fbits;

    result._raw = (int32_t)prod;
    assert(prod == (int64_t)result._raw);

    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_div(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    assert(b._raw != 0);
    struct ui_fx result;
    result.is_init = true;

    int64_t num = (int64_t)a._raw << ui_fx_fbits;
    int64_t quot = num / b._raw;

    result._raw = (int32_t)quot;
    assert(quot == (int64_t)result._raw);

    return result;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_min(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    return a._raw < b._raw ? a : b;
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_max(struct ui_fx a, struct ui_fx b) {
    assert(a.is_init);
    assert(b.is_init);
    return a._raw > b._raw ? a : b;
}

cr_hdef __attribute__((always_inline))
bool ui_fx_is_positive(struct ui_fx x) {
    assert(x.is_init);
    return x._raw > 0;
}

cr_hdef __attribute__((always_inline))
int32_t ui_fx_get_px(struct ui_fx x) {
    assert(x.is_init);
    return (x._raw + 32) >> ui_fx_fbits;
}

cr_hdef __attribute__((always_inline))
int32_t ui_fx_to_text_266(struct ui_fx x, double to_px_from_pt) {
    assert(x.is_init);
    return (int32_t)floor((double)x._raw / to_px_from_pt);
}

cr_hdef __attribute__((always_inline))
struct ui_fx ui_fx_from_text_266(int32_t v_266, double to_px_from_pt) {
    double px = (double)v_266 * to_px_from_pt * (1.0 / 64.0);
    return ui_fx_make((int32_t)ceil(px));
}

cr_hdef __attribute__((always_inline))
double ui_round_px_ex(double value, double to_px_from_pt) {
    return (double)ui_fx_get_px(ui_fx_make_px(value, to_px_from_pt)) / to_px_from_pt;
}

cr_hdef __attribute__((always_inline))
double ui_round_px_fx_ex(double value, double to_px_from_pt) {
    return (double)ui_fx_get_px(ui_fx_make_round(value * to_px_from_pt)) / to_px_from_pt;
}

cr_hdef __attribute__((always_inline))
double ui_round_px_size_ex(double value, double to_px_from_pt) {
    return (double)ui_fx_get_px(ui_fx_make_px_size(value, to_px_from_pt)) / to_px_from_pt;
}

cr_hdef __attribute__((always_inline))
double ui_approach_px_ex(double current, double target, double f, double to_px_from_pt) {
    double delta_px = round((target - current) * to_px_from_pt);

    if (delta_px == 0.0) {
        return target;
    }

    if (f <= 0.0) {
        return current;
    }

    double step_px = round(delta_px * f);

    if (step_px == 0.0) {
        step_px = delta_px > 0.0 ? 1.0 : -1.0;
    }

    return target - (delta_px - step_px) / to_px_from_pt;
}
