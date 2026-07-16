#include "demo_icons.h"

static void demo_icon_path(struct ui_node parent, double box, const struct ui_path* path, double thickness, struct ui_color color) {
    ui_shape(&(struct ui_shape) {
        .parent = parent,
        .size = ui_size_fix(box, box),
        .mode = ui_shape_mode_stroke,
        .thickness = thickness,
        .path = path,
        .fill = ui_fill_solid(color),
    });
}

static void demo_icon_ring(struct ui_node parent, double2 center, double radius, struct ui_color color) {
    ui_ellipse(&(struct ui_ellipse) {
        .parent = parent,
        .pos = ui_pos_xy(center.x - radius, center.y - radius),
        .size = ui_size_fix(radius * 2.0, radius * 2.0),
        .outline = ui_outline_solid(ui_outline_align_center, 1.0, color),
    });
}

static void demo_icon_draw_laptop(struct ui_node parent, struct ui_color color) {
    ui_rect(&(struct ui_rect) {
        .parent = parent,
        .pos = ui_pos_xy(2.5, 3.5),
        .size = ui_size_fix(11.0, 8.0),
        .corners = ui_corners(2.0),
        .outline = ui_outline_solid(ui_outline_align_center, 1.0, color),
    });

    ui_line(&(struct ui_line) {
        .parent = parent,
        .pos = ui_pos_xy(5.0, 13.0),
        .size = ui_size_fix(6.0, 1.0),
        .thickness = 1.0,
        .fill = ui_fill_solid(color),
    });
}

static void demo_icon_draw_folder(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(2.75, 4.0));
    ui_path_line_to(path, make_double2(5.7, 4.0));
    ui_path_quad_to(path, make_double2(6.35, 4.0), make_double2(6.8, 4.45));
    ui_path_line_to(path, make_double2(8.0, 5.65));
    ui_path_quad_to(path, make_double2(8.45, 6.0), make_double2(9.0, 6.0));
    ui_path_line_to(path, make_double2(13.25, 6.0));
    ui_path_quad_to(path, make_double2(14.5, 6.0), make_double2(14.5, 7.25));
    ui_path_line_to(path, make_double2(14.5, 11.25));
    ui_path_quad_to(path, make_double2(14.5, 12.5), make_double2(13.25, 12.5));
    ui_path_line_to(path, make_double2(2.75, 12.5));
    ui_path_quad_to(path, make_double2(1.5, 12.5), make_double2(1.5, 11.25));
    ui_path_line_to(path, make_double2(1.5, 5.25));
    ui_path_quad_to(path, make_double2(1.5, 4.0), make_double2(2.75, 4.0));
    ui_path_close(path);

    demo_icon_path(parent, 16, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_branch(struct ui_node parent, struct ui_color color) {
    demo_icon_ring(parent, make_double2(4.5, 4.5), 2.0, color);
    demo_icon_ring(parent, make_double2(4.5, 11.5), 2.0, color);
    demo_icon_ring(parent, make_double2(11.5, 4.5), 2.0, color);

    ui_line(&(struct ui_line) {
        .parent = parent,
        .pos = ui_pos_xy(4.0, 7.0),
        .size = ui_size_fix(1.0, 2.0),
        .thickness = 1.0,
        .fill = ui_fill_solid(color),
    });

    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(11.5, 6.75));
    ui_path_cubic_to(path, make_double2(11.5, 8.6), make_double2(9.0, 9.5), make_double2(6.25, 9.5));

    demo_icon_path(parent, 16, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_plus(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(7.5, 2.5));
    ui_path_line_to(path, make_double2(7.5, 12.5));
    ui_path_move_to(path, make_double2(2.5, 7.5));
    ui_path_line_to(path, make_double2(12.5, 7.5));

    demo_icon_path(parent, 16, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_external(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(6.5, 4.5));
    ui_path_line_to(path, make_double2(3.5, 4.5));
    ui_path_quad_to(path, make_double2(2.5, 4.5), make_double2(2.5, 5.5));
    ui_path_line_to(path, make_double2(2.5, 12.5));
    ui_path_quad_to(path, make_double2(2.5, 13.5), make_double2(3.5, 13.5));
    ui_path_line_to(path, make_double2(10.5, 13.5));
    ui_path_quad_to(path, make_double2(11.5, 13.5), make_double2(11.5, 12.5));
    ui_path_line_to(path, make_double2(11.5, 9.5));

    ui_path_move_to(path, make_double2(7.5, 8.5));
    ui_path_line_to(path, make_double2(13.0, 3.0));

    ui_path_move_to(path, make_double2(9.5, 2.5));
    ui_path_line_to(path, make_double2(13.5, 2.5));
    ui_path_line_to(path, make_double2(13.5, 6.5));

    demo_icon_path(parent, 16, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_chevron_right(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(6.5, 4.5));
    ui_path_line_to(path, make_double2(10.0, 8.0));
    ui_path_line_to(path, make_double2(6.5, 11.5));

    demo_icon_path(parent, 16, path, 1.5, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_check(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(3.5, 8.5));
    ui_path_line_to(path, make_double2(6.5, 11.5));
    ui_path_line_to(path, make_double2(13.0, 5.0));

    demo_icon_path(parent, 16, path, 1.5, color);
    ui_path_destroy(path);
}

void demo_icon_draw(struct ui_node parent, enum demo_icon icon, struct ui_color color) {
    switch (icon) {
    case demo_icon_none: break;
    case demo_icon_laptop: demo_icon_draw_laptop(parent, color); break;
    case demo_icon_folder: demo_icon_draw_folder(parent, color); break;
    case demo_icon_branch: demo_icon_draw_branch(parent, color); break;
    case demo_icon_plus: demo_icon_draw_plus(parent, color); break;
    case demo_icon_external: demo_icon_draw_external(parent, color); break;
    case demo_icon_chevron_right: demo_icon_draw_chevron_right(parent, color); break;
    case demo_icon_check: demo_icon_draw_check(parent, color); break;
    }
}

static void demo_icon_draw_laptop_large(struct ui_node parent, struct ui_color color) {
    ui_rect(&(struct ui_rect) {
        .parent = parent,
        .pos = ui_pos_xy(2.5, 3.5),
        .size = ui_size_fix(13.0, 9.0),
        .corners = ui_corners(2.0),
        .outline = ui_outline_solid(ui_outline_align_center, 1.0, color),
    });

    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(7.0, 15.5));
    ui_path_line_to(path, make_double2(11.0, 15.5));

    demo_icon_path(parent, 18, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_folder_large(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(3.5, 3.5));
    ui_path_line_to(path, make_double2(6.75, 3.5));
    ui_path_quad_to(path, make_double2(7.45, 3.5), make_double2(7.95, 4.0));
    ui_path_line_to(path, make_double2(9.45, 5.5));
    ui_path_quad_to(path, make_double2(9.95, 6.0), make_double2(10.65, 6.0));
    ui_path_line_to(path, make_double2(14.5, 6.0));
    ui_path_quad_to(path, make_double2(15.5, 6.0), make_double2(15.5, 7.0));
    ui_path_line_to(path, make_double2(15.5, 13.5));
    ui_path_quad_to(path, make_double2(15.5, 14.5), make_double2(14.5, 14.5));
    ui_path_line_to(path, make_double2(3.5, 14.5));
    ui_path_quad_to(path, make_double2(2.5, 14.5), make_double2(2.5, 13.5));
    ui_path_line_to(path, make_double2(2.5, 4.5));
    ui_path_quad_to(path, make_double2(2.5, 3.5), make_double2(3.5, 3.5));
    ui_path_close(path);

    demo_icon_path(parent, 18, path, 1.0, color);
    ui_path_destroy(path);
}

static void demo_icon_draw_branch_large(struct ui_node parent, struct ui_color color) {
    demo_icon_ring(parent, make_double2(5.5, 4.5), 2.0, color);
    demo_icon_ring(parent, make_double2(5.5, 13.5), 2.0, color);
    demo_icon_ring(parent, make_double2(13.5, 4.5), 2.0, color);

    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(5.5, 7.0));
    ui_path_line_to(path, make_double2(5.5, 11.0));

    ui_path_move_to(path, make_double2(13.5, 7.0));
    ui_path_cubic_to(path, make_double2(13.5, 9.75), make_double2(9.5, 11.0), make_double2(5.5, 11.0));

    demo_icon_path(parent, 18, path, 1.0, color);
    ui_path_destroy(path);
}

void demo_icon_draw_large(struct ui_node parent, enum demo_icon icon, struct ui_color color) {
    switch (icon) {
    case demo_icon_laptop: demo_icon_draw_laptop_large(parent, color); break;
    case demo_icon_folder: demo_icon_draw_folder_large(parent, color); break;
    case demo_icon_branch: demo_icon_draw_branch_large(parent, color); break;
    default: assert(0); break;
    }
}

void demo_icon_draw_check_small(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(3.5, 7.5));
    ui_path_line_to(path, make_double2(6.0, 10.0));
    ui_path_line_to(path, make_double2(10.5, 5.5));

    demo_icon_path(parent, 14, path, 1.5, color);
    ui_path_destroy(path);
}

void demo_icon_draw_send_arrow(struct ui_node parent, struct ui_color color) {
    struct ui_path* path = ui_path_create();

    ui_path_move_to(path, make_double2(10.0, 15.0));
    ui_path_line_to(path, make_double2(16.0, 9.0));
    ui_path_line_to(path, make_double2(22.0, 15.0));

    ui_path_move_to(path, make_double2(16.0, 9.0));
    ui_path_line_to(path, make_double2(16.0, 23.0));

    demo_icon_path(parent, 32, path, 2.0, color);
    ui_path_destroy(path);
}

void demo_icon_draw_help_ring(struct ui_node parent, struct ui_color color) {
    demo_icon_ring(parent, make_double2(8.0, 8.0), 7.0, color);
}
