#include <stdio.h>
#include <stdlib.h>

#include "cr.h"
#include "ui.h"
#include "ui_core.h"
#include "ui_corner.h"

void ui_path_init(struct ui_path* path, struct cr_mem* mem) {
    *path = (struct ui_path) { .mem = mem };
}

void ui_path_init_copy(struct ui_path* path, struct cr_mem* mem, const struct ui_path* src) {
    *path = (struct ui_path) {
        .mem = mem,
        .count = src->count,
        .capacity = src->count,
        .position = src->position,
        .subpath_start = src->subpath_start,
    };

    if (src->count > 0) {
        size_t size = src->count * sizeof(*path->components);
        path->components = cr_malloc(mem, size, 0, cr_loc);
        memcpy(path->components, src->components, size);
    }
}

void ui_path_deinit(struct ui_path* path) {
    if (path->components) {
        cr_free(path->mem, path->components, cr_loc);
    }
}

struct ui_path* ui_path_create(void) {
    struct ui* ui = ui_ctx();
    struct ui_path* path = cr_malloc(ui->mem, sizeof(*path), 0, cr_loc);
    ui_path_init(path, ui->mem);
    return path;
}

void ui_path_destroy(struct ui_path* path) {
    struct ui* ui = ui_ctx();
    ui_path_deinit(path);
    cr_free(ui->mem, path, cr_loc);
}

uint32_t ui_path_get_count(const struct ui_path* path) {
    return path->count;
}

const struct ui_path_component* ui_path_get_components(const struct ui_path* path) {
    return path->components;
}

void ui_path_append(struct ui_path* path, const struct ui_path_component* components, uint32_t count) {
    if (count == 0) {
        return;
    }

    uint32_t new_count = path->count + count;
    if (new_count > path->capacity) {
        path->capacity = max_uint32(16u, next_po2_uint32(new_count));
        path->components = cr_realloc(path->mem, path->components, path->capacity * sizeof(*path->components), cr_loc);
    }
    memcpy(path->components + path->count, components, count * sizeof(*components));
    path->count = new_count;

    for (uint32_t i = 0; i < count; i++) {
        const struct ui_path_component* component = &components[i];

        switch (component->type) {
        case ui_path_component_type_move: {
            path->position = component->move.p;
            path->subpath_start = component->move.p;
        } break;
        case ui_path_component_type_line: {
            path->position = component->line.p;
        } break;
        case ui_path_component_type_cubic: {
            path->position = component->cubic.p;
        } break;
        case ui_path_component_type_close: {
            path->position = path->subpath_start;
        } break;
        }
    }
}

void ui_path_move_to(struct ui_path* path, double2 p) {
    ui_path_append(path, &(struct ui_path_component) { .type = ui_path_component_type_move, .move.p = p }, 1);
}

void ui_path_line_to(struct ui_path* path, double2 p) {
    ui_path_append(path, &(struct ui_path_component) { .type = ui_path_component_type_line, .line.p = p }, 1);
}

void ui_path_cubic_to(struct ui_path* path, double2 c1, double2 c2, double2 p) {
    ui_path_append(path, &(struct ui_path_component) { .type = ui_path_component_type_cubic, .cubic = { .c1 = c1, .c2 = c2, .p = p } }, 1);
}

void ui_path_quad_to(struct ui_path* path, double2 c, double2 p) {
    double2 c1 = path->position + (c - path->position) * (2.0 / 3.0);
    double2 c2 = p + (c - p) * (2.0 / 3.0);
    ui_path_cubic_to(path, c1, c2, p);
}

void ui_path_close(struct ui_path* path) {
    ui_path_append(path, &(struct ui_path_component) { .type = ui_path_component_type_close }, 1);
}

void ui_path_arc_to_cubics(double2 center, double radius, double start_angle, double end_angle, bool clockwise, double2 out_cubics[][4], uint32_t* out_cubic_count) {
    double sweep = fmod(end_angle - start_angle, (2.0 * M_PI));

    if (clockwise) {
        if (sweep > 0.0) {
            sweep -= (2.0 * M_PI);
        }
    } else {
        if (sweep < 0.0) {
            sweep += (2.0 * M_PI);
        }
    }

    if (fabs(end_angle - start_angle) >= (2.0 * M_PI - 1e-5)) {
        if (clockwise) {
            sweep = -(2.0 * M_PI);
        } else {
            sweep = (2.0 * M_PI);
        }
    } else {
        if (sweep == 0.0) {
            if (fabs(end_angle - start_angle) > 1e-5) {
                if (clockwise) {
                    sweep = -(2.0 * M_PI);
                } else {
                    sweep = (2.0 * M_PI);
                }
            }
        }
    }

    if (sweep == 0.0) {
        *out_cubic_count = 0;
        return;
    }

    uint32_t segment_count = (uint32_t)clamp_double(ceil(fabs(sweep) / M_PI_2), 0.0, 4.0);
    double segment_sweep = sweep / (double)segment_count;
    double kappa = (4.0 / 3.0) * tan(0.25 * segment_sweep);

    double current_angle = start_angle;
    double2 start_point;
    start_point.x = center.x + radius * cos(start_angle);
    start_point.y = center.y + radius * sin(start_angle);

    *out_cubic_count = segment_count;

    for (uint32_t i = 0; i < segment_count; i++) {
        double next_angle = current_angle + segment_sweep;
        double sin_next_angle = sin(next_angle);
        double cos_next_angle = cos(next_angle);
        double sin_current_angle = sin(current_angle);
        double cos_current_angle = cos(current_angle);

        double2 control_1;
        control_1.x = start_point.x - radius * (kappa * sin_current_angle);
        control_1.y = start_point.y + radius * (kappa * cos_current_angle);

        double2 end_point;
        end_point.x = center.x + radius * cos_next_angle;
        end_point.y = center.y + radius * sin_next_angle;

        double2 control_2;
        control_2.x = end_point.x + radius * (kappa * sin_next_angle);
        control_2.y = end_point.y - radius * (kappa * cos_next_angle);

        out_cubics[i][0] = start_point;
        out_cubics[i][1] = control_1;
        out_cubics[i][2] = control_2;
        out_cubics[i][3] = end_point;

        start_point = end_point;
        current_angle = next_angle;
    }
}

void ui_path_arc(struct ui_path* path, double2 center, double radius, double start_angle, double end_angle, bool clockwise) {
    double2 cubics[4][4];
    uint32_t cubic_count = 0;

    ui_path_arc_to_cubics(center, radius, start_angle, end_angle, clockwise, cubics, &cubic_count);

    if (cubic_count == 0) {
        return;
    }

    double2 start = cubics[0][0];
    if (path->count == 0) {
        ui_path_move_to(path, start);
    } else if (is_equal_double2(path->position, start) == false) {
        ui_path_line_to(path, start);
    }

    for (uint32_t i = 0; i < cubic_count; i++) {
        ui_path_cubic_to(path, cubics[i][1], cubics[i][2], cubics[i][3]);
    }
}

void ui_path_rounded_rect(struct ui_path* path, double2 origin, double2 size, struct ui_corners corners) {
    size = max_double2(make_double2(0.0, 0.0), size);
    double4 r = corners.radius;

    double2 points[4] = {
        origin + make_double2(size.x, 0.0),
        origin + size,
        origin + make_double2(0.0, size.y),
        origin,
    };
    double radii[4] = { r.y, r.w, r.z, r.x };

    ui_path_move_to(path, origin);
    ui_path_corners(path, points, radii, 4, corners.profile == ui_corner_profile_continuous);
}

void ui_path_corners(struct ui_path* path, const double2* points, const double* radii, uint32_t count, bool continuous) {
    cr_check(count >= 1 && count < ui_path_corners_max, "path corners take between 1 and %u points", (uint32_t)(ui_path_corners_max - 1));

    double2 anchor = path->position;
    bool closed = count >= 3 && is_equal_double2(points[count - 1], anchor);
    uint32_t n = closed ? count : count + 1;

    double2 pts[ui_path_corners_max];
    double rs[ui_path_corners_max];

    pts[0] = anchor;
    rs[0] = closed ? max_double(0.0, radii[count - 1]) : 0.0;
    for (uint32_t j = 1; j < n; j++) {
        pts[j] = points[j - 1];
        rs[j] = max_double(0.0, radii[j - 1]);
    }

    ui_corner_path(path, pts, rs, n, closed, continuous);
}

#define ui_ellipse_kappa 0.55228474983

void ui_path_ellipse(struct ui_path* path, double2 origin, double2 size) {
    double2 r = size * 0.5;
    double2 c = origin + r;
    double2 k = r * ui_ellipse_kappa;

    ui_path_move_to(path, make_double2(c.x, c.y - r.y));
    ui_path_cubic_to(path, make_double2(c.x + k.x, c.y - r.y), make_double2(c.x + r.x, c.y - k.y), make_double2(c.x + r.x, c.y));
    ui_path_cubic_to(path, make_double2(c.x + r.x, c.y + k.y), make_double2(c.x + k.x, c.y + r.y), make_double2(c.x, c.y + r.y));
    ui_path_cubic_to(path, make_double2(c.x - k.x, c.y + r.y), make_double2(c.x - r.x, c.y + k.y), make_double2(c.x - r.x, c.y));
    ui_path_cubic_to(path, make_double2(c.x - r.x, c.y - k.y), make_double2(c.x - k.x, c.y - r.y), make_double2(c.x, c.y - r.y));
    ui_path_close(path);
}

static bool ui_path_control_polygon_is_smooth(double2 p0, double2 p1, double2 p2, double2 p3, double cos_max_turn) {
    double2 edges[3] = { p1 - p0, p2 - p1, p3 - p2 };
    double threshold = fabs(cos_max_turn) * cos_max_turn;

    bool has_prev = false;
    double2 prev = { 0 };
    double prev_len_sq = 0;

    for (int i = 0; i < 3; i++) {
        double len_sq = edges[i].x * edges[i].x + edges[i].y * edges[i].y;
        if (len_sq < 1e-12) {
            continue;
        }

        if (has_prev) {
            double d = prev.x * edges[i].x + prev.y * edges[i].y;
            if (fabs(d) * d < threshold * prev_len_sq * len_sq) {
                return false;
            }
        }

        has_prev = true;
        prev = edges[i];
        prev_len_sq = len_sq;
    }

    return true;
}

static void ui_path_flatten_cubic(struct ui_arr_double2* points, double2 p0, double2 p1, double2 p2, double2 p3, double epsilon_sq, double cos_max_turn, int depth) {
    if (depth > 16) {
        ui_arr_double2_append(points, p3);
        return;
    }

    double2 d = p3 - p0;
    double len_sq = d.x * d.x + d.y * d.y;
    double d1, d2;

    if (len_sq == 0.0) {
        double2 v1 = p1 - p0;
        double2 v2 = p2 - p0;
        d1 = v1.x * v1.x + v1.y * v1.y;
        d2 = v2.x * v2.x + v2.y * v2.y;
    } else {
        double2 v1 = p1 - p0;
        double2 v2 = p2 - p0;
        double cross1 = d.x * v1.y - d.y * v1.x;
        double cross2 = d.x * v2.y - d.y * v2.x;
        d1 = (cross1 * cross1) / len_sq;
        d2 = (cross2 * cross2) / len_sq;
    }

    if (d1 <= epsilon_sq && d2 <= epsilon_sq && ui_path_control_polygon_is_smooth(p0, p1, p2, p3, cos_max_turn)) {
        ui_arr_double2_append(points, p3);
        return;
    }

    double2 p01 = (p0 + p1) * 0.5;
    double2 p12 = (p1 + p2) * 0.5;
    double2 p23 = (p2 + p3) * 0.5;
    double2 p012 = (p01 + p12) * 0.5;
    double2 p123 = (p12 + p23) * 0.5;
    double2 p0123 = (p012 + p123) * 0.5;

    ui_path_flatten_cubic(points, p0, p01, p012, p0123, epsilon_sq, cos_max_turn, depth + 1);
    ui_path_flatten_cubic(points, p0123, p123, p23, p3, epsilon_sq, cos_max_turn, depth + 1);
}

void ui_path_to_points(const struct ui_path* path, double2 offset, double scale, struct ui_arr_double2* points, struct ui_arr_uint32* contour_starts, double epsilon, double max_angle) {
    double epsilon_sq = epsilon * epsilon;
    double cos_max_turn = cos(0.5 * max_angle);
    double2 position = offset;
    double2 subpath_start = offset;
    bool contour_open = false;

    for (uint32_t i = 0; i < path->count; i++) {
        const struct ui_path_component* component = &path->components[i];

        if ((component->type == ui_path_component_type_line || component->type == ui_path_component_type_cubic) && contour_open == false) {
            if (contour_starts) {
                ui_arr_uint32_append(contour_starts, ui_arr_double2_get_count(points));
            }
            ui_arr_double2_append(points, position);
            subpath_start = position;
            contour_open = true;
        }

        switch (component->type) {
        case ui_path_component_type_move: {
            position = offset + component->move.p * scale;
            contour_open = false;
        } break;
        case ui_path_component_type_line: {
            position = offset + component->line.p * scale;
            ui_arr_double2_append(points, position);
        } break;
        case ui_path_component_type_cubic: {
            double2 p = offset + component->cubic.p * scale;
            ui_path_flatten_cubic(points, position, offset + component->cubic.c1 * scale, offset + component->cubic.c2 * scale, p, epsilon_sq, cos_max_turn, 0);
            position = p;
        } break;
        case ui_path_component_type_close: {
            if (contour_open && is_equal_double2(position, subpath_start) == false) {
                ui_arr_double2_append(points, subpath_start);
            }
            position = subpath_start;
            contour_open = false;
        } break;
        }
    }
}

bool ui_path_get_bounds(const struct ui_path* path, double2* out_min, double2* out_max) {
    double2 bounds_min = make_double2(INFINITY, INFINITY);
    double2 bounds_max = make_double2(-INFINITY, -INFINITY);
    bool has_segment = false;
    double2 position = make_double2(0.0, 0.0);
    double2 subpath_start = position;

    for (uint32_t i = 0; i < path->count; i++) {
        const struct ui_path_component* component = &path->components[i];

        switch (component->type) {
        case ui_path_component_type_move: {
            position = component->move.p;
            subpath_start = position;
        } break;
        case ui_path_component_type_line: {
            bounds_min = min_double2(bounds_min, min_double2(position, component->line.p));
            bounds_max = max_double2(bounds_max, max_double2(position, component->line.p));
            position = component->line.p;
            has_segment = true;
        } break;
        case ui_path_component_type_cubic: {
            bounds_min = min_double2(bounds_min, min_double2(position, component->cubic.c1));
            bounds_max = max_double2(bounds_max, max_double2(position, component->cubic.c1));
            bounds_min = min_double2(bounds_min, min_double2(component->cubic.c2, component->cubic.p));
            bounds_max = max_double2(bounds_max, max_double2(component->cubic.c2, component->cubic.p));
            position = component->cubic.p;
            has_segment = true;
        } break;
        case ui_path_component_type_close: {
            position = subpath_start;
        } break;
        }
    }

    *out_min = bounds_min;
    *out_max = bounds_max;

    return has_segment;
}
