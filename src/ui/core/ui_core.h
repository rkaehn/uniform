#pragma once

#include "ui.h"
#include "ui_node.h"
#include "gpu.h"

struct ui_atlas;
struct ui_persistent;
struct ui_transient;
struct ui_mod_text;
struct ui_render;

struct ui {
    struct cr_mem* mem;
    struct gpu_device* device;
    double to_px_from_pt;
    struct ui_node_pool node_pool;
    struct ui_mod_text* mod_text;
    struct ui_atlas* atlas;
    struct ui_persistent* persistent;
    struct ui_transient* transient;
    struct ui_render* render;
};

extern struct ui* ui_context;

cr_hdef struct ui* ui_ctx(void) {
    cr_check(ui_context != NULL, "the ui is not initialized; call ui_init first");
    return ui_context;
}

cr_hdef struct ui_node_backing* ui_node_check(struct ui* ui, struct ui_node node, const char* what) {
    cr_check(ui_node_is_null(node) == false, "%s is null", what);
    cr_check(ui_is_alive(node), "%s is not alive", what);
    return &ui->node_pool.items[ui_node_get_idx(node)];
}
