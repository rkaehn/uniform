#include <math.h>
#include <stdio.h>
#include "cr.h"
#include "ui_corner.h"
#include "ui_sdf_shared.h"
#include "ui_tests.h"

static double2 ui_test_corner_bez(const double2* p, double t) {
    double u = 1.0 - t;
    return p[0] * (u * u * u) + p[1] * (3.0 * u * u * t) + p[2] * (3.0 * u * t * t) + p[3] * (t * t * t);
}

void ui_test_corner_(void) {
    uint64_t state = 0;
    bool ok = true;
    double worst = 0.0;

    for (uint32_t iter = 0; iter < 200 && ok; iter++) {
        double2 origin = make_double2(16.0, 16.0);
        double2 size = make_double2(8.0 + 792.0 * cr_2u01(cr_rand(&state)),
                                    8.0 + 792.0 * cr_2u01(cr_rand(&state)));
        double4 radius;
        for (uint32_t k = 0; k < 4; k++) {
            radius[k] = cr_rand(&state) % 5 == 0 ? 0.0 : 300.0 * cr_2u01(cr_rand(&state));
        }
        bool continuous = (cr_rand(&state) & 1) != 0;

        struct ui_sdf_analytic_prim prim;
        ui_corner_analytic(origin, size, make_double4(radius.x, radius.y, radius.w, radius.z),
                           continuous, make_double2(0.0, 0.0), &prim);

        struct ui_path* path = ui_path_create();
        ui_path_rounded_rect(path, origin, size, (struct ui_corners) {
            .profile = continuous ? ui_corner_profile_continuous : ui_corner_profile_circular,
            .radius = radius,
        });

        double r_max = max_double(max_double(radius.x, radius.y), max_double(radius.z, radius.w));
        double r_eff = min_double(min_double(r_max, 0.5 * size.x), 0.5 * size.y);
        double limit = 0.0008 * r_eff + 0.02;

        double dev = 0.0;
        double grad_err = 0.0;
        uint32_t samples = 0;
        double2 pos = make_double2(0.0, 0.0);
        double2 anchor = make_double2(0.0, 0.0);

        const struct ui_path_component* components = ui_path_get_components(path);

        for (uint32_t i = 0, component_count = ui_path_get_count(path); i < component_count; i++) {
            const struct ui_path_component* c = &components[i];
            double2 probes[31];
            uint32_t probe_count = 0;

            switch (c->type) {
            case ui_path_component_type_move:
                pos = c->move.p;
                anchor = pos;
                break;
            case ui_path_component_type_line:
                probes[probe_count++] = (pos + c->line.p) * 0.5;
                pos = c->line.p;
                break;
            case ui_path_component_type_cubic: {
                double2 cubic[4] = { pos, c->cubic.c1, c->cubic.c2, c->cubic.p };
                for (uint32_t s = 1; s < 32; s++) {
                    probes[probe_count++] = ui_test_corner_bez(cubic, (double)s / 32.0);
                }
                pos = c->cubic.p;
            } break;
            case ui_path_component_type_close:
                probes[probe_count++] = (pos + anchor) * 0.5;
                pos = anchor;
                break;
            }

            for (uint32_t s = 0; s < probe_count; s++) {
                float3 f = ui_sdf_analytic_prim_field(prim.pos, prim.cell_x, prim.cell_y, prim.coef,
                                                     make_float2((float)probes[s].x, (float)probes[s].y), 1e9f);
                dev = max_double(dev, fabs((double)f.x));
                grad_err = max_double(grad_err, fabs(sqrt((double)(f.y * f.y + f.z * f.z)) - 1.0));
                samples += 1;
            }
        }

        ui_path_destroy(path);

        worst = max_double(worst, dev / limit);

        if (samples == 0 || dev > limit || grad_err > 2e-3) {
            printf("test_corner: fuzz %u FAILED size (%.1f, %.1f) radius (%.1f, %.1f, %.1f, %.1f) %s %u samples, dev %.5f (limit %.5f), grad %.5f\n",
                   iter, size.x, size.y, radius.x, radius.y, radius.z, radius.w,
                   continuous ? "continuous" : "circular", samples, dev, limit, grad_err);
            ok = false;
        }
    }

    printf("test_corner: %s (worst dev %.2f of limit)\n", ok ? "ok" : "FAILED", worst);
    assert(ok);
}
