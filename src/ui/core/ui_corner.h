#pragma once

#include "cr.h"

struct ui_sdf_analytic_prim;

struct ui_corner_circle {
    double rho;
    double2 center_in;
    double2 center_out;
};

struct ui_corner_arm {
    double start;
    double mid;
    double foot;
    double2 arc_start;
    double psi;
    bool has_blend;
};

struct ui_corner_geom {
    double2 vertex;
    double2 edge_in;
    double2 normal_in;
    double2 edge_out;
    double2 normal_out;
    double turn;
    struct ui_corner_circle circle;
    struct ui_corner_arm arm_in;
    struct ui_corner_arm arm_out;
};

struct ui_corner_rect_corner {
    uint32_t cubic_count;
    double2 start;
    double2 cell;
    double2 cubics[6][4];
};

struct ui_path;

void ui_corner_grants(const double2* points, const double* radii, uint32_t count, bool closed, bool continuous, double* out_claim, double* out_desired, double* out_grant_in, double* out_grant_out);
bool ui_corner_geom(double2 prev, double2 vertex, double2 next, double claim, double grant_in, double grant_out, struct ui_corner_geom* out_geom);
uint32_t ui_corner_arc_cubics(double2 center, double rho, double tangent_from, double tangent_to, double2 out_cubics[2][4]);
uint32_t ui_corner_geom_cubics(const struct ui_corner_geom* geom, double2 out_cubics[6][4], double2* out_start);

void ui_corner_path(struct ui_path* path, const double2* points, const double* radii, uint32_t count, bool closed, bool continuous);
void ui_corner_rect(double2 origin, double2 size, double4 radius_tl_tr_br_bl, bool continuous, struct ui_corner_rect_corner out_corners[4]);
void ui_corner_analytic(double2 origin, double2 size, double4 radius_tl_tr_br_bl, bool continuous, double2 to_px_offset, struct ui_sdf_analytic_prim* out_prim);
