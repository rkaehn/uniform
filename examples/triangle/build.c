#include "bd.h"
#include <stdio.h>

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    struct bd* bd = bd_create(cr_mem_libc, "../..");

    struct bd_config* config = bd_config_create_default(bd, NULL);
    struct bd_config* config_app = bd_config_create_default(bd, &(struct bd_config_desc) { .bundle_identifier = "com.example.triangle" });

    struct bd_target* shaders = bd_target_create(bd, "triangle_shaders", bd_target_type_shaders);
    bd_target_set_config(shaders, config);
    bd_target_add_input(shaders, "triangle.hlsl");

    struct bd_target* shaders_embed = bd_target_create(bd, "triangle_shaders_embed", bd_target_type_embed);
    bd_target_set_config(shaders_embed, config);
    bd_target_add_input_ex(shaders_embed, &(struct bd_input_desc) { .target = shaders });

    struct bd_target* triangle = bd_target_create(bd, "triangle", bd_target_type_app);
    bd_target_set_config(triangle, config_app);
    bd_target_add_input(triangle, "main.c");
    bd_target_add_input_ex(triangle, &(struct bd_input_desc) { .target = shaders_embed });
    bd_add_lib_uniform(triangle, bd_module_all);

    bd_set_defaults(bd, &(struct bd_defaults_desc) { .target = triangle, .platform = bd_platform_macos, .profile = bd_profile_release });

    return bd_run(bd, argc, argv);
}
