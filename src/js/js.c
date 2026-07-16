#include "js_internal.h"
#include "js_gen.h"
#include <stdlib.h>

#define js_arena_size (js_layout_scratch_size + js_layout_bump_size)

uint8_t* js_arena;
double* js_args;
static uint32_t js_bump_cursor;

void js_arena_init(void) {
    assert(js_arena == NULL && "js_arena_init is called once");
    js_arena = cr_malloc(cr_mem_libc, js_arena_size, 16, cr_loc);
    js_args = (double*)js_arena;
    js_bump_cursor = js_layout_scratch_size;
}

double js_arena_base(void) {
    assert(js_arena != NULL && "js_arena_init");
    return js_ptr(js_arena);
}

double js_malloc(uint32_t size) {
    assert(size > 0);
    void* p = malloc(size);
    assert(p != NULL);
    return js_ptr(p);
}

void js_free(double ptr) {
    free(js_p(ptr));
}

void js_memcpy(double dst, double src, uint32_t size) {
    if (size > 0) {
        memcpy(js_p(dst), js_p(src), size);
    }
}

uint32_t js_strlen(double ptr) {
    return (uint32_t)strlen(js_p(ptr));
}

double js_mem_libc(void) {
    return js_ptr(cr_mem_libc);
}

void* js_bump(uint32_t size, uint32_t align) {
    assert(js_arena != NULL && "js_arena_init");
    assert(align != 0 && (align & (align - 1)) == 0 && align <= 16);
    uint32_t off = (js_bump_cursor + align - 1) & ~(align - 1);
    assert(off <= js_arena_size && size <= js_arena_size - off && "callback argument overflow");
    js_bump_cursor = off + size;
    return js_arena + off;
}

uint32_t js_bump_mark(void) {
    return js_bump_cursor;
}

void js_bump_release(uint32_t mark) {
    assert(mark >= js_layout_scratch_size && mark <= js_bump_cursor);
    js_bump_cursor = mark;
}

double js_cb_addr(uint32_t kind) {
    assert(kind < js_site_count);
    cr_check(js_tramp_addrs[kind] != NULL, "the callback site is not available on this platform");
    return js_ptr(js_tramp_addrs[kind]);
}

void js_ui_custom_draw_solid(double draw, double transform, double color, uint32_t vertex_count, double vertices) {
    cr_check(draw != 0.0, "customDrawSolid takes the info passed to onRender");
    if (vertex_count == 0) {
        return;
    }
    void (*draw_solid)(float3x2, struct ui_color, uint32_t, struct ui_custom_vertex_solid**) = js_p(draw);
    struct ui_custom_vertex_solid* dst;
    draw_solid(*(const float3x2*)js_p(transform), *(const struct ui_color*)js_p(color), vertex_count, &dst);
    memcpy(dst, js_p(vertices), vertex_count * sizeof(*dst));
}

void js_ui_custom_draw_graphic(double draw, double transform, double graphic, double tint, uint32_t vertex_count, double vertices) {
    cr_check(draw != 0.0, "customDrawGraphic takes the info passed to onRender");
    if (vertex_count == 0) {
        return;
    }
    void (*draw_graphic)(float3x2, struct ui_graphic*, struct ui_color, uint32_t, struct ui_custom_vertex_graphic**) = js_p(draw);
    struct ui_custom_vertex_graphic* dst;
    draw_graphic(*(const float3x2*)js_p(transform), js_p(graphic), *(const struct ui_color*)js_p(tint), vertex_count, &dst);
    memcpy(dst, js_p(vertices), vertex_count * sizeof(*dst));
}
