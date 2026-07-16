#include "tests_internal.h"
#include "app.h"
#include "app_tests.h"
#include "log.h"
#include "log_tests.h"
#include "cr_tests.h"
#include "prim_tests.h"
#include "tree_tests.h"
#include "hud_tests.h"
#include "ui_input.h"
#include "hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if cr_ios
#import <Foundation/Foundation.h>
#endif

cr_warning_push
cr_warning_ignore_everything
static const uint8_t _jetbrains[] = {
#embed "../assets/JetBrainsMono.ttf"
};
static const uint8_t _inter[] = {
#embed "../assets/Inter.ttf"
};
cr_warning_pop

enum tests_mode {
    tests_mode_interactive,
    tests_mode_test,
    tests_mode_capture,
};

struct tests {
    enum tests_mode mode;
    uint32_t iterations;
    float screen_to_px_from_pt;
    const char* capture_only;
    bool remote_log;
    struct ui_font* font;
    struct ui_test_scenes* scenes;
    struct app_window* window;
    bool did_init;
    bool did_run;
    bool pass;
    uint32_t init_allocs;
    double begin_s;
};

static struct tests tests_ = {
    .iterations = 1,
    .screen_to_px_from_pt = 2.0f,
};

static void tests_init(struct tests* tests, struct gpu_device* device) {
    if (tests->did_init) {
        return;
    }
    tests->did_init = true;
    tests->init_allocs = cr_rec_get_alloc_count();

    ui_init(cr_mem_libc, device);
    tests->font = ui_font_create(_jetbrains, sizeof(_jetbrains));
    ui_set_default_font(tests->font);
    tests->scenes = ui_test_scenes_create_(cr_mem_libc, &(struct ui_test_scenes_desc) {
        .font_data = _jetbrains, .font_size = sizeof(_jetbrains),
        .font_ui_data = _inter, .font_ui_size = sizeof(_inter),
        .screen_to_px_from_pt = tests->screen_to_px_from_pt,
    });
    ui_input_set(ui_input_create(cr_mem_libc));
    hud_set(hud_create(cr_mem_libc, device, tests->font));
    hud_set_enabled(true);
}

static void tests_deinit(struct tests* tests) {
    ui_test_scenes_destroy_(tests->scenes);
    tests->scenes = NULL;
    hud_destroy(hud_get());
    ui_input_destroy(ui_input_get());
    ui_font_destroy(tests->font);
    ui_deinit();
    tests->did_init = false;
}

const uint8_t* ui_test_font_data_(bool ui_font, uint64_t* out_size) {
    *out_size = ui_font ? sizeof(_inter) : sizeof(_jetbrains);
    return ui_font ? _inter : _jetbrains;
}

static void tests_hud(void) {
    hud_test(tests_.font);
}

static void tests_refs(void) {
    tests_refs_test_(tests_.scenes, tests_.iterations, &tests_.pass);
}

static void tests_perf(void) {
    ui_test_perf_(tests_.iterations);
}

static void (*const tests_cases_[])(void) = {
    cr_test_dict,
    cr_test_pool,
    cr_test_mem,
    cr_test_arr,
    cr_test_str,
    cr_test_util,
    tree_test,
    prim_test,
    cr_test_task,
    ui_test_text_,
    ui_test_list_,
    ui_test_layout_,
    ui_test_render_,
    ui_test_atlas_,
    ui_test_text_input_,
    ui_test_input_,
    ui_test_key_,
    ui_test_transient_,
    ui_test_field_,
    ui_test_edr_,
    ui_test_containment_,
    ui_test_shelf_,
    ui_test_corner_,
    ui_test_yoga_,
    tests_perf,
    tests_hud,
    log_test,
    tests_refs,
};

static void tests_run(struct app* app);

static void tests_on_update(struct app_window* window) {
    struct tests* tests = app_window_get_user(window);

    if (tests->mode == tests_mode_test) {
        if (tests->did_run == false) {
            tests->did_run = true;
            tests_run(app_window_get_app(window));
        }
        return;
    }

    if (tests->mode == tests_mode_capture) {
        assert(cr_ios);
#if cr_ios
        if (tests->did_run) {
            return;
        }
        tests->did_run = true;

        struct app_target_desc target = app_window_get_target_desc(window);
        CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(NULL, CFAbsoluteTimeGetCurrent(), 0.0, 0, 0, ^(CFRunLoopTimerRef fired) {
            tests_refs_capture_(tests->scenes, target.to_px_from_pt, target.pt_size, tests->capture_only);
        });
        CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
        CFRelease(timer);
        return;
#else
        exit(1);
#endif
    }

    tests_browser_frame_(window, tests->scenes);
}

static void tests_on_close_request(struct app_window* window) {
    app_quit(app_window_get_app(window));
}

static void tests_window_create(struct tests* tests, struct app* app) {
    tests->window = app_window_create(app, &(struct app_window_desc) {
        .user = tests,
        .on_update = tests_on_update,
        .on_close_request = tests_on_close_request,
        .title = "tests",
        .size = make_double2(1280.0, 800.0),
        .center = true,
        .resizable = true,
    });
}

static void tests_finish(void* user) {
    struct app* app = user;
    struct tests* tests = app_get_user(app);

    bool windows_pass = true;
// #if cr_macos || cr_wasm
//     windows_pass = app_test_core_(app) && windows_pass;
// #endif
// #if cr_macos
//     windows_pass = app_test_window_(app) && windows_pass;
// #endif

    tests_deinit(tests);

    if (cr_rec_get_alloc_count() != tests->init_allocs) {
        printf("leaks: %u allocs before the suite, %u after\n", tests->init_allocs, cr_rec_get_alloc_count());
        cr_rec_print_allocs();
    }
    assert(cr_rec_get_alloc_count() == tests->init_allocs);

    printf("finished in %.2fs\n", cr_get_current_time() - tests->begin_s);
    printf("result: %s\n", tests->pass && windows_pass ? "pass" : "fail");

#if cr_wasm
    if (tests->remote_log == false) {
        tests->mode = tests_mode_interactive;
        tests_init(tests, app_get_device(app));
        tests_window_create(tests, app);
        return;
    }
#endif
    exit(tests->pass && windows_pass ? 0 : 1);
}

static void tests_run(struct app* app) {
    struct tests* tests = app_get_user(app);
    tests_init(tests, app_get_device(app));
    tests->begin_s = cr_get_current_time();
    ui_test_run_(app_get_device(app), tests_cases_, cr_count(tests_cases_), tests_finish, app);
}

static void tests_on_quit(struct app* app) {
    struct tests* tests = app_get_user(app);
    if (tests->did_init) {
        tests_deinit(tests);
    }
}

static void tests_on_start(struct app* app) {
    struct tests* tests = app_get_user(app);

    if (tests->mode == tests_mode_test) {
#if cr_ios
        tests_window_create(tests, app);
#else
        tests_run(app);
#endif
        return;
    }

    tests_init(tests, app_get_device(app));
    tests_window_create(tests, app);
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            tests_.mode = tests_mode_test;
        } else if (strcmp(argv[i], "--capture") == 0) {
            tests_.mode = tests_mode_capture;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                tests_.capture_only = argv[++i];
            }
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            int n = atoi(argv[++i]);
            tests_.iterations = n > 1 ? (uint32_t)n : 1;
        } else if (strcmp(argv[i], "--ref") == 0 && i + 1 < argc) {
            tests_.screen_to_px_from_pt = tests_refs_use_(argv[++i]);
        } else if (strcmp(argv[i], "--remote-log") == 0) {
            tests_.remote_log = true;
        }
    }

    if (tests_.mode == tests_mode_interactive) {
        log_init(NULL);
        log_set_mode(log_mode_term);
    }

    app_run(&(struct app_desc) {
        .mem = cr_mem_libc,
        .argc = argc,
        .argv = argv,
        .user = &tests_,
        .on_start = tests_on_start,
        .on_quit = tests_on_quit,
    });
    return 0;
}
