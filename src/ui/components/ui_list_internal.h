#pragma once

#include "cr.h"
#include "ui_list.h"

cr_hdef double ui_list_component(double2 v, enum ui_list_axis axis) {
    return axis == ui_list_axis_x ? v.x : v.y;
}

cr_hdef double2 ui_list_from_components(enum ui_list_axis axis, double along, double across) {
    return axis == ui_list_axis_x ? make_double2(along, across) : make_double2(across, along);
}

cr_hdef enum ui_list_axis ui_list_cross_axis(enum ui_list_axis axis) {
    return axis == ui_list_axis_x ? ui_list_axis_y : ui_list_axis_x;
}
