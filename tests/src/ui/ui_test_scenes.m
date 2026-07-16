#include "ui_tests.h"

static const struct ui_test_scene* (*const ui_test_scene_segments_[])(struct ui_test_scenes* scenes, uint32_t* out_count) = {
    ui_test_scenes_health_,
    ui_test_scenes_color_,
    ui_test_scenes_text_,
    ui_test_scenes_rect_,
    ui_test_scenes_path_,
    ui_test_scenes_layer_,
    ui_test_scenes_shadow_,
    ui_test_scenes_blur_,
    ui_test_scenes_glass_,
};

static struct ui_test_scene ui_test_scene_for_screen(struct ui_test_scene scene, float screen_to_px_from_pt) {
    assert(scene.capture == ui_test_scene_capture_pixels || scene.capture == ui_test_scene_capture_screenshot);
    assert(scene.build != NULL);
    assert((scene.capture == ui_test_scene_capture_screenshot) == (scene.to_px_from_pt == 0.0f));
    if (scene.capture == ui_test_scene_capture_screenshot) {
        scene.to_px_from_pt = screen_to_px_from_pt;
    }
    double2 px = scene.size * (double)scene.to_px_from_pt;
    assert(is_equal_double2(px, floor_double2(px)));
    scene.width = (uint32_t)px.x;
    scene.height = (uint32_t)px.y;
    return scene;
}

struct ui_test_scenes* ui_test_scenes_create_(struct cr_mem* mem, const struct ui_test_scenes_desc* desc) {
    cr_check(desc->screen_to_px_from_pt > 0.0f, "scenes need the scale of the screen that shows them");
    struct ui_test_scenes* scenes = cr_malloc(mem, sizeof(*scenes), 16, cr_loc);
    *scenes = (struct ui_test_scenes) {
        .mem = mem,
        .desc = *desc,
    };
    ui_test_scenes_glass_build_(scenes);

    for (uint32_t i = 0; i < cr_count(ui_test_scene_segments_); i++) {
        uint32_t count;
        ui_test_scene_segments_[i](scenes, &count);
        scenes->count += count;
    }
    scenes->list = cr_malloc(mem, scenes->count * sizeof(*scenes->list), 16, cr_loc);
    uint32_t index = 0;
    for (uint32_t i = 0; i < cr_count(ui_test_scene_segments_); i++) {
        uint32_t count;
        const struct ui_test_scene* segment = ui_test_scene_segments_[i](scenes, &count);
        for (uint32_t j = 0; j < count; j++) {
            scenes->list[index++] = ui_test_scene_for_screen(segment[j], desc->screen_to_px_from_pt);
        }
    }
    return scenes;
}

void ui_test_scenes_destroy_(struct ui_test_scenes* scenes) {
    cr_free(scenes->mem, scenes->list, cr_loc);
    cr_free(scenes->mem, scenes->glass_scenes, cr_loc);
    cr_free(scenes->mem, scenes->glass_captures, cr_loc);
    cr_free(scenes->mem, scenes, cr_loc);
}

uint32_t ui_test_scene_count_(struct ui_test_scenes* scenes) {
    return scenes->count;
}

const struct ui_test_scene* ui_test_scene_get_(struct ui_test_scenes* scenes, uint32_t index) {
    assert(index < scenes->count);
    return &scenes->list[index];
}

struct ui_test_scene_state* ui_test_scene_create_(struct ui_test_scenes* scenes, struct cr_mem* mem, const struct ui_test_scene* scene) {
    if (scene->create != NULL) {
        return scene->create(scenes, mem, scene->ctx);
    }

    struct ui_test_scene_state* state = cr_malloc(mem, sizeof(*state), 16, cr_loc);
    *state = (struct ui_test_scene_state) {
        .scenes = scenes,
        .mem = mem,
        .destroy = ui_test_scene_state_free_,
    };
    return state;
}

const uint8_t* ui_test_scenes_font_(const struct ui_test_scenes* scenes, bool ui_font, uint64_t* out_size) {
    const uint8_t* data = ui_font ? scenes->desc.font_ui_data : scenes->desc.font_data;
    assert(data != NULL);
    *out_size = ui_font ? scenes->desc.font_ui_size : scenes->desc.font_size;
    return data;
}
