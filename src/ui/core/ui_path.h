#pragma once

#include "cr.h"
#include "ui.h"

#define cr_arr ui_arr_double2
#define cr_arr_item double2
#include "cr_arr.h"

#define cr_arr ui_arr_uint32
#define cr_arr_item uint32_t
#include "cr_arr.h"

struct ui_path {
    struct cr_mem* mem;
    struct ui_path_component* components;
    uint32_t count;
    uint32_t capacity;
    double2 position;
    double2 subpath_start;
};

void ui_path_init(struct ui_path* path, struct cr_mem* mem);
void ui_path_init_copy(struct ui_path* path, struct cr_mem* mem, const struct ui_path* src);
void ui_path_deinit(struct ui_path* path);

void ui_path_arc_to_cubics(double2 center, double radius, double start_angle, double end_angle, bool clockwise, double2 out_cubics[][4], uint32_t* out_cubic_count);

void ui_path_to_points(const struct ui_path* path, double2 offset, double scale, struct ui_arr_double2* points, struct ui_arr_uint32* contour_starts, double epsilon, double max_angle);

bool ui_path_get_bounds(const struct ui_path* path, double2* out_min, double2* out_max);
