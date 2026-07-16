#include "cr.h"
#include "ui.h"
#include "ui_corner.h"
#include "ui_path.h"
#include "ui_sdf_shared.h"

#define ui_corner_eps_ 1e-9
#define ui_corner_blend_turn_share_ (2.0 / 9.0)

static double ui_corner_dot_(double2 a, double2 b) {
    return a.x * b.x + a.y * b.y;
}

static double ui_corner_cross_(double2 a, double2 b) {
    return a.x * b.y - a.y * b.x;
}

static double ui_corner_length_(double2 a) {
    return sqrt(ui_corner_dot_(a, a));
}

static double ui_corner_hover_(double psi) {
    double p = psi * psi;
    return p / 6.0 - p * p / 168.0 + p * p * p / 7920.0;
}

static double ui_corner_blend_span_(double psi, double rho) {
    double p = psi * psi;
    return 2.0 * psi * rho * (1.0 - p / 10.0 + p * p / 216.0);
}

static double ui_corner_contact_(double radius, double turn) {
    return radius * tan(0.5 * turn);
}

static double ui_corner_free_radius_(double radius, double turn) {
    double sec = 1.0 / cos(0.5 * turn);
    double inset = 1.0 + ui_corner_hover_(ui_corner_blend_turn_share_ * turn);
    return radius * (sec - 1.0) / (inset * sec - 1.0);
}

static double ui_corner_free_footprint_(double radius, double turn) {
    double psi = ui_corner_blend_turn_share_ * turn;
    double rho = ui_corner_free_radius_(radius, turn);
    double inset = rho * (1.0 + ui_corner_hover_(psi));
    return inset * tan(0.5 * turn) - rho * sin(psi) + ui_corner_blend_span_(psi, rho);
}

static struct ui_corner_arm ui_corner_arm_(struct ui_corner_circle circle, double2 center, double radius, double turn, double psi, double grant) {
    double rho = circle.rho;
    double2 arc_start = center - make_double2(sin(psi), cos(psi)) * rho;

    if (psi < 1e-7 || arc_start.y < 1e-12 * radius) {
        return (struct ui_corner_arm) {
            .start = arc_start.x,
            .mid = arc_start.x,
            .foot = arc_start.x,
            .arc_start = arc_start,
            .psi = psi,
            .has_blend = false,
        };
    }

    double foot = arc_start.x + arc_start.y * cos(psi) / sin(psi);
    double handle = arc_start.y / sin(psi);
    double mid = foot + 1.5 * handle * handle / (rho * sin(psi));
    double2 d1 = (arc_start - make_double2(foot, 0.0)) * 3.0;
    double2 d2 = (arc_start - make_double2(2.0 * foot - mid, 0.0)) * 6.0;
    double2 d3 = (arc_start - make_double2(3.0 * (foot - mid), 0.0)) * 6.0;
    double cross12 = ui_corner_cross_(d1, d2);
    double cross13 = ui_corner_cross_(d1, d3);
    double g3_start = (3.0 * cross12 * ui_corner_dot_(d1, d2) / ui_corner_dot_(d1, d1) - cross13) / (6.0 * d1.y);
    double start = min_double(grant, max_double(g3_start, mid + 0.05 * (mid - foot)));

    return (struct ui_corner_arm) {
        .start = start,
        .mid = mid,
        .foot = foot,
        .arc_start = arc_start,
        .psi = psi,
        .has_blend = true,
    };
}

static double ui_corner_full_footprint_(double radius, double turn) {
    double psi = ui_corner_blend_turn_share_ * turn;
    double rho = ui_corner_free_radius_(radius, turn);
    double inset = 1.0 + ui_corner_hover_(psi);
    double cx = inset * (1.0 - cos(turn)) / sin(turn);
    struct ui_corner_circle circle = {
        .rho = rho,
        .center_in = make_double2(cx, inset) * rho,
        .center_out = make_double2(cx, inset) * rho,
    };
    return ui_corner_arm_(circle, circle.center_in, radius, turn, psi, INFINITY).start;
}

static double ui_corner_compression_(double radius, double turn, double grant) {
    double tight = ui_corner_contact_(radius, turn);
    double free = ui_corner_free_footprint_(radius, turn);
    if (free > tight * (1.0 + 1e-12)) {
        return clamp_double((free - grant) / (free - tight), 0.0, 1.0);
    }
    return 1.0;
}

static double ui_corner_coupling_(double compression_in, double compression_out) {
    double c = max_double(compression_in, compression_out);
    return 1.0 - (1.0 - c) * (1.0 - c);
}

static double ui_corner_blend_turn_(double grant, double rho, double cx, double limit) {
    double target = grant / rho - cx;
    if (target <= 0.0) {
        return 0.0;
    }
    double psi = target;
    for (int i = 0; i < 3; i++) {
        double p = psi * psi;
        psi = target / (1.0 - p / 30.0 + p * p / 1080.0);
    }
    return min_double(psi, limit);
}

static struct ui_corner_circle ui_corner_circle_(double radius, double turn, double psi_in, double psi_out, double m) {
    double inset_in = 1.0 + ui_corner_hover_(psi_in);
    double inset_out = 1.0 + ui_corner_hover_(psi_out);

    double cx_in = (inset_out - inset_in * cos(turn)) / sin(turn);
    double cx_out = (inset_in - inset_out * cos(turn)) / sin(turn);

    double apex_per_rho = sqrt(cx_in * cx_in + inset_in * inset_in) - 1.0;
    double apex_target = radius * (1.0 / cos(0.5 * turn) - 1.0);

    double touch_per_rho = psi_in <= psi_out ? cx_in : cx_out;
    double touch_target = radius * tan(0.5 * turn);

    double rho = ((1.0 - m) * apex_target + m * touch_target)
               / ((1.0 - m) * apex_per_rho + m * touch_per_rho);

    return (struct ui_corner_circle) {
        .rho = rho,
        .center_in = make_double2(cx_in, inset_in) * rho,
        .center_out = make_double2(cx_out, inset_out) * rho,
    };
}

static double ui_corner_turn_(double2 din, double2 dout) {
    return clamp_double(atan2(fabs(ui_corner_cross_(din, dout)), ui_corner_dot_(din, dout)), 0.0, M_PI - 1e-3);
}

static double ui_corner_edge_(const double2* points, uint32_t count, bool closed, uint32_t edge, double2* out_dir) {
    *out_dir = make_double2(1.0, 0.0);
    if (closed == false && edge == 0) {
        return 0.0;
    }
    double2 d = points[edge] - points[(edge + count - 1) % count];
    double len = ui_corner_length_(d);
    if (len > ui_corner_eps_) {
        *out_dir = d * (1.0 / len);
    }
    return len;
}

void ui_corner_grants(const double2* points, const double* radii, uint32_t count, bool closed, bool continuous,
                      double* out_claim, double* out_desired, double* out_grant_in, double* out_grant_out) {
    for (uint32_t j = 0; j < count; j++) {
        double2 din;
        double2 dout;
        double len_in = ui_corner_edge_(points, count, closed, j, &din);
        double len_out = ui_corner_edge_(points, count, closed, (j + 1) % count, &dout);
        double turn = ui_corner_turn_(din, dout);

        bool degenerate = len_in <= ui_corner_eps_
            || len_out <= ui_corner_eps_
            || turn <= 1e-6
            || radii[j] <= 0.0;
        out_claim[j] = degenerate ? 0.0 : ui_corner_contact_(radii[j], turn);
        out_desired[j] = out_claim[j];
    }

    for (uint32_t j = 0; j < count; j++) {
        uint32_t next = (j + 1) % count;
        double2 dir;
        double length = ui_corner_edge_(points, count, closed, next, &dir);
        double a = out_desired[j];
        double b = out_desired[next];

        if (a + b > length && a + b > ui_corner_eps_) {
            double scale = length / (a + b);

            out_claim[j] = min_double(out_claim[j], a * scale);
            out_claim[next] = min_double(out_claim[next], b * scale);
        }
    }

    for (uint32_t j = 0; j < count; j++) {
        out_desired[j] = 0.0;
        if (continuous == false || out_claim[j] <= ui_corner_eps_) {
            continue;
        }
        double2 din;
        double2 dout;
        ui_corner_edge_(points, count, closed, j, &din);
        ui_corner_edge_(points, count, closed, (j + 1) % count, &dout);
        double turn = ui_corner_turn_(din, dout);
        double radius = out_claim[j] / tan(0.5 * turn);
        double free = ui_corner_full_footprint_(radius, turn);

        out_desired[j] = max_double(0.0, free - out_claim[j]);
    }

    for (uint32_t j = 0; j < count; j++) {
        uint32_t next = (j + 1) % count;
        double2 dir;
        double length = ui_corner_edge_(points, count, closed, next, &dir);
        double slack = max_double(0.0, length - (out_claim[j] + out_claim[next]));
        double wanted = out_desired[j] + out_desired[next];
        double fraction = wanted > ui_corner_eps_ ? min_double(1.0, slack / wanted) : 1.0;

        out_grant_out[j] = out_claim[j] + fraction * out_desired[j];
        out_grant_in[next] = out_claim[next] + fraction * out_desired[next];
    }
}

bool ui_corner_geom(double2 prev, double2 vertex, double2 next, double claim, double grant_in, double grant_out,
                    struct ui_corner_geom* out_geom) {
    if (claim <= ui_corner_eps_) {
        return false;
    }

    double2 din = vertex - prev;
    double2 dout = next - vertex;
    din = din * (1.0 / ui_corner_length_(din));
    dout = dout * (1.0 / ui_corner_length_(dout));

    double side = ui_corner_cross_(din, dout) < 0.0 ? -1.0 : 1.0;
    double turn = ui_corner_turn_(din, dout);

    double radius = claim / tan(0.5 * turn);
    double compression_in = ui_corner_compression_(radius, turn, grant_in);
    double compression_out = ui_corner_compression_(radius, turn, grant_out);
    double m = ui_corner_coupling_(compression_in, compression_out);
    double psi_max = ui_corner_blend_turn_share_ * turn;
    double psi_in = psi_max * (1.0 - compression_in);
    double psi_out = psi_max * (1.0 - compression_out);

    struct ui_corner_circle circle = ui_corner_circle_(radius, turn, psi_in, psi_out, m);

    for (int i = 0; i < 2; i++) {
        psi_in = ui_corner_blend_turn_(grant_in, circle.rho, circle.center_in.x / circle.rho, psi_max);
        psi_out = ui_corner_blend_turn_(grant_out, circle.rho, circle.center_out.x / circle.rho, psi_max);
        circle = ui_corner_circle_(radius, turn, psi_in, psi_out, m);
    }

    out_geom->vertex = vertex;
    out_geom->edge_in = -din;
    out_geom->normal_in = make_double2(-din.y * side, din.x * side);
    out_geom->edge_out = dout;
    out_geom->normal_out = make_double2(-dout.y * side, dout.x * side);
    out_geom->turn = turn;
    out_geom->circle = circle;
    out_geom->arm_in = ui_corner_arm_(circle, circle.center_in, radius, turn, psi_in, grant_in);
    out_geom->arm_out = ui_corner_arm_(circle, circle.center_out, radius, turn, psi_out, grant_out);
    return true;
}

uint32_t ui_corner_arc_cubics(double2 center, double rho, double tangent_from, double tangent_to, double2 out_cubics[2][4]) {
    uint32_t piece_count = tangent_to - tangent_from > 0.5 ? 2 : 1;

    for (uint32_t i = 0; i < piece_count; i++) {
        double s = tangent_from + (tangent_to - tangent_from) * (double)i / (double)piece_count;
        double e = tangent_from + (tangent_to - tangent_from) * (double)(i + 1) / (double)piece_count;
        double2 p0 = center - make_double2(sin(s), cos(s)) * rho;
        double2 p3 = center - make_double2(sin(e), cos(e)) * rho;
        double handle = (4.0 / 3.0) * tan(0.25 * (e - s)) * rho;
        double2 t0 = make_double2(-cos(s), sin(s));
        double2 t1 = make_double2(-cos(e), sin(e));
        out_cubics[i][0] = p0;
        out_cubics[i][1] = p0 + t0 * handle;
        out_cubics[i][2] = p3 - t1 * handle;
        out_cubics[i][3] = p3;
    }

    return piece_count;
}

static double2 ui_corner_world_(const struct ui_corner_geom* geom, double2 edge, double2 normal, double2 p) {
    return geom->vertex + edge * p.x + normal * p.y;
}

uint32_t ui_corner_geom_cubics(const struct ui_corner_geom* geom, double2 out_cubics[6][4], double2* out_start) {
    uint32_t n = 0;
    double half = 0.5 * geom->turn;
    double2 arcs[2][4];

    if (geom->arm_in.has_blend) {
        out_cubics[n][0] = ui_corner_world_(geom, geom->edge_in, geom->normal_in, make_double2(geom->arm_in.start, 0.0));
        out_cubics[n][1] = ui_corner_world_(geom, geom->edge_in, geom->normal_in, make_double2(geom->arm_in.mid, 0.0));
        out_cubics[n][2] = ui_corner_world_(geom, geom->edge_in, geom->normal_in, make_double2(geom->arm_in.foot, 0.0));
        out_cubics[n][3] = ui_corner_world_(geom, geom->edge_in, geom->normal_in, geom->arm_in.arc_start);
        *out_start = out_cubics[n][0];
        n++;
    } else {
        *out_start = ui_corner_world_(geom, geom->edge_in, geom->normal_in, geom->arm_in.arc_start);
    }

    uint32_t in_count = ui_corner_arc_cubics(geom->circle.center_in, geom->circle.rho, geom->arm_in.psi, half, arcs);
    for (uint32_t k = 0; k < in_count; k++) {
        for (uint32_t j = 0; j < 4; j++) {
            out_cubics[n][j] = ui_corner_world_(geom, geom->edge_in, geom->normal_in, arcs[k][j]);
        }
        n++;
    }

    uint32_t out_count = ui_corner_arc_cubics(geom->circle.center_out, geom->circle.rho, geom->arm_out.psi, half, arcs);
    for (uint32_t k = out_count; k-- > 0;) {
        for (uint32_t j = 0; j < 4; j++) {
            out_cubics[n][j] = ui_corner_world_(geom, geom->edge_out, geom->normal_out, arcs[k][3 - j]);
        }
        n++;
    }

    if (geom->arm_out.has_blend) {
        out_cubics[n][0] = ui_corner_world_(geom, geom->edge_out, geom->normal_out, geom->arm_out.arc_start);
        out_cubics[n][1] = ui_corner_world_(geom, geom->edge_out, geom->normal_out, make_double2(geom->arm_out.foot, 0.0));
        out_cubics[n][2] = ui_corner_world_(geom, geom->edge_out, geom->normal_out, make_double2(geom->arm_out.mid, 0.0));
        out_cubics[n][3] = ui_corner_world_(geom, geom->edge_out, geom->normal_out, make_double2(geom->arm_out.start, 0.0));
        n++;
    }

    return n;
}

static void ui_corner_path_emit_(struct ui_path* path, bool* started, double2 p) {
    if (*started) {
        if (is_equal_double2(p, path->position) == false) {
            ui_path_line_to(path, p);
        }
    } else {
        ui_path_move_to(path, p);
        *started = true;
    }
}

void ui_corner_path(struct ui_path* path, const double2* points, const double* radii, uint32_t count, bool closed, bool continuous) {
    assert(count >= 2 && count <= ui_path_corners_max);

    double claim[ui_path_corners_max];
    double desired[ui_path_corners_max];
    double grant_in[ui_path_corners_max];
    double grant_out[ui_path_corners_max];

    ui_corner_grants(points, radii, count, closed, continuous, claim, desired, grant_in, grant_out);

    bool started = closed == false;

    for (uint32_t j = 0; j < count; j++) {
        struct ui_corner_geom geom;

        if (ui_corner_geom(points[(j + count - 1) % count], points[j], points[(j + 1) % count],
                           claim[j], grant_in[j], grant_out[j], &geom) == false) {
            ui_corner_path_emit_(path, &started, points[j]);
            continue;
        }

        double2 cubics[6][4];
        double2 start;
        uint32_t cubic_count = ui_corner_geom_cubics(&geom, cubics, &start);

        ui_corner_path_emit_(path, &started, start);
        for (uint32_t k = 0; k < cubic_count; k++) {
            ui_path_cubic_to(path, cubics[k][1], cubics[k][2], cubics[k][3]);
        }
    }

    if (closed) {
        ui_path_close(path);
    }
}

void ui_corner_rect(double2 origin, double2 size, double4 radius, bool continuous, struct ui_corner_rect_corner out_corners[4]) {
    size = max_double2(make_double2(0.0, 0.0), size);

    double2 points[4] = {
        origin,
        origin + make_double2(size.x, 0.0),
        origin + size,
        origin + make_double2(0.0, size.y),
    };
    double radii[4];
    for (int i = 0; i < 4; i++) {
        radii[i] = max_double(0.0, radius[i]);
    }

    double claim[4];
    double desired[4];
    double grant_in[4];
    double grant_out[4];
    ui_corner_grants(points, radii, 4, true, continuous, claim, desired, grant_in, grant_out);

    for (int i = 0; i < 4; i++) {
        struct ui_corner_rect_corner* corner = &out_corners[i];
        struct ui_corner_geom geom;

        if (ui_corner_geom(points[(i + 3) % 4], points[i], points[(i + 1) % 4],
                           claim[i], grant_in[i], grant_out[i], &geom) == false) {
            corner->cubic_count = 0;
            corner->start = points[i];
            corner->cell = make_double2(0.0, 0.0);
            continue;
        }

        corner->cubic_count = ui_corner_geom_cubics(&geom, corner->cubics, &corner->start);
        corner->cell = (i & 1) ? make_double2(geom.arm_in.start, geom.arm_out.start)
                               : make_double2(geom.arm_out.start, geom.arm_in.start);
    }
}

static double2 ui_corner_bezier_(const double2 p[4], double t) {
    double u = 1.0 - t;
    return p[0] * (u * u * u) + p[1] * (3.0 * u * u * t) + p[2] * (3.0 * u * t * t) + p[3] * (t * t * t);
}

static bool ui_corner_solve_(double a[ui_sdf_analytic_coef_count][ui_sdf_analytic_coef_count], double b[ui_sdf_analytic_coef_count],
                             double out_x[ui_sdf_analytic_coef_count]) {
    int n = ui_sdf_analytic_coef_count;

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int row = col + 1; row < n; row++) {
            if (fabs(a[row][col]) > fabs(a[pivot][col])) {
                pivot = row;
            }
        }
        if (fabs(a[pivot][col]) < 1e-14) {
            return false;
        }
        if (pivot != col) {
            for (int k = col; k < n; k++) {
                double tmp = a[col][k];
                a[col][k] = a[pivot][k];
                a[pivot][k] = tmp;
            }
            double tmp = b[col];
            b[col] = b[pivot];
            b[pivot] = tmp;
        }
        for (int row = col + 1; row < n; row++) {
            double f = a[row][col] / a[col][col];
            for (int k = col; k < n; k++) {
                a[row][k] -= f * a[col][k];
            }
            b[row] -= f * b[col];
        }
    }

    for (int row = n; row-- > 0;) {
        double s = b[row];
        for (int k = row + 1; k < n; k++) {
            s -= a[row][k] * out_x[k];
        }
        out_x[row] = s / a[row][row];
    }
    return true;
}

#define ui_corner_project_steps_ 12
#define ui_corner_project_passes_ 6
#define ui_corner_project_max_ (6 * (ui_corner_project_steps_ + 1))

static void ui_corner_project_(const struct ui_corner_rect_corner* corner, double2 center, double2 half, double mx, double my,
                               double out_coef[ui_sdf_analytic_coef_count]) {
    double2 cell = corner->cell;
    double2 cell_origin = half - cell;

    double rows[ui_corner_project_max_][ui_sdf_analytic_coef_count];
    double targets[ui_corner_project_max_];
    double weights[ui_corner_project_max_];
    double residuals[ui_corner_project_max_];
    uint32_t sample_count = 0;

    for (uint32_t c = 0; c < corner->cubic_count; c++) {
        for (int s = 0; s <= ui_corner_project_steps_; s++) {
            double t = (double)s / (double)ui_corner_project_steps_;
            double2 p = ui_corner_bezier_(corner->cubics[c], t);
            double2 q = make_double2((p.x - center.x) * mx, (p.y - center.y) * my);
            double2 u = (q - cell_origin) / cell;
            double ux = max_double(u.x, 0.0);
            double uy = max_double(u.y, 0.0);
            double radial = sqrt(ux * ux + uy * uy);
            if (radial < 1e-9) {
                continue;
            }
            double dx = ux / radial;
            double dy = uy / radial;
            double x = dx * dx - dy * dy;
            double y = max_double(2.0 * dx * dy, 0.0);
            double w = 0.25 * y * y;
            if (w < 1e-10) {
                continue;
            }

            double gx = dx / cell.x;
            double gy = dy / cell.y;
            double px_scale = 1.0 / max_double(1e-9, sqrt(gx * gx + gy * gy));

            assert(sample_count < ui_corner_project_max_);
            double* row = rows[sample_count];
            double ck = 1.0;
            double sk = 0.0;
            row[0] = w * px_scale;
            for (int k = 1; k < ui_sdf_analytic_coef_count; k++) {
                double ck_next = ck * x - sk * y;
                sk = sk * x + ck * y;
                ck = ck_next;
                row[k] = w * ck * px_scale;
            }
            targets[sample_count] = (radial - 1.0) * px_scale;
            weights[sample_count] = 1.0;
            sample_count++;
        }
    }

    for (int pass = 0; pass < ui_corner_project_passes_; pass++) {
        double gram[ui_sdf_analytic_coef_count][ui_sdf_analytic_coef_count] = { 0 };
        double rhs[ui_sdf_analytic_coef_count] = { 0 };

        for (uint32_t n = 0; n < sample_count; n++) {
            const double* row = rows[n];
            double wn = weights[n];
            double wt = wn * targets[n];
            for (int k = 0; k < ui_sdf_analytic_coef_count; k++) {
                double wk = wn * row[k];
                rhs[k] += row[k] * wt;
                for (int l = k; l < ui_sdf_analytic_coef_count; l++) {
                    gram[k][l] += wk * row[l];
                }
            }
        }
        for (int k = 1; k < ui_sdf_analytic_coef_count; k++) {
            for (int l = 0; l < k; l++) {
                gram[k][l] = gram[l][k];
            }
        }

        double solved[ui_sdf_analytic_coef_count] = { 0 };
        if (ui_corner_solve_(gram, rhs, solved) == false) {
            break;
        }
        memcpy(out_coef, solved, ui_sdf_analytic_coef_count * sizeof(double));

        if (pass == ui_corner_project_passes_ - 1) {
            break;
        }

        double residual_max = 0.0;
        for (uint32_t n = 0; n < sample_count; n++) {
            double r = -targets[n];
            for (int k = 0; k < ui_sdf_analytic_coef_count; k++) {
                r += rows[n][k] * out_coef[k];
            }
            residuals[n] = fabs(r);
            residual_max = max_double(residual_max, residuals[n]);
        }
        if (residual_max < 0.01) {
            break;
        }
        for (uint32_t n = 0; n < sample_count; n++) {
            weights[n] *= 0.1 + residuals[n] / residual_max;
        }
    }
}

void ui_corner_analytic(double2 origin, double2 size, double4 radius, bool continuous, double2 to_px_offset, struct ui_sdf_analytic_prim* out_prim) {
    struct ui_corner_rect_corner corners[4];
    ui_corner_rect(origin, size, radius, continuous, corners);

    double2 half = make_double2(max_double(0.0, 0.5 * size.x), max_double(0.0, 0.5 * size.y));
    double2 center = origin + half;

    memset(out_prim, 0, sizeof(*out_prim));
    out_prim->pos = make_float4((float)(center.x + to_px_offset.x), (float)(center.y + to_px_offset.y),
                                (float)half.x, (float)half.y);

    bool uniform = radius.x == radius.y && radius.x == radius.z && radius.x == radius.w;
    double coef0[ui_sdf_analytic_coef_count] = { 0 };
    bool have0 = false;

    for (int i = 0; i < 4; i++) {
        double2 cell = corners[i].cell;
        out_prim->cell_x[i] = (float)cell.x;
        out_prim->cell_y[i] = (float)cell.y;

        if (continuous == false || cell.x <= 1e-6 || cell.y <= 1e-6) {
            continue;
        }

        double mx = (i == 0 || i == 3) ? -1.0 : 1.0;
        double my = i < 2 ? -1.0 : 1.0;

        double coef[ui_sdf_analytic_coef_count] = { 0 };
        if (uniform && have0) {
            memcpy(coef, coef0, sizeof(coef));
        } else {
            ui_corner_project_(&corners[i], center, half, mx, my, coef);
            if (uniform) {
                memcpy(coef0, coef, sizeof(coef0));
                have0 = true;
            }
        }

        for (int k = 0; k < ui_sdf_analytic_coef_count; k++) {
            out_prim->coef[2 * i + k / 4][k % 4] = (float)coef[k];
        }
    }
}
