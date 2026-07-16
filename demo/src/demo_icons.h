#pragma once

#include "cr.h"
#include "ui.h"

enum demo_icon {
    demo_icon_none,
    demo_icon_laptop,
    demo_icon_folder,
    demo_icon_branch,
    demo_icon_plus,
    demo_icon_external,
    demo_icon_chevron_right,
    demo_icon_check,
};

void demo_icon_draw(struct ui_node parent, enum demo_icon icon, struct ui_color color);
void demo_icon_draw_large(struct ui_node parent, enum demo_icon icon, struct ui_color color);
void demo_icon_draw_check_small(struct ui_node parent, struct ui_color color);
void demo_icon_draw_send_arrow(struct ui_node parent, struct ui_color color);
void demo_icon_draw_help_ring(struct ui_node parent, struct ui_color color);
