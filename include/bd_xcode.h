#pragma once

#include "bd.h"

struct bd_xcode_dependency_desc {
    const char* project;
    const char* target;
    uint32_t filter;
    bool embed;
};

void bd_xcode_set_project_name(struct bd* bd, const char* name);
void bd_xcode_set_command_prefix(struct bd* bd, const char* command);
void bd_xcode_add_group_order(struct bd* bd, const char* group);
void bd_xcode_add_target(struct bd* bd, struct bd_target* target);
void bd_xcode_add_dependency(struct bd* bd, struct bd_target* target, const struct bd_xcode_dependency_desc* desc);
