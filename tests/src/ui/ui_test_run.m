#include "ui_tests.h"

#define ui_test_runner_steps 64u

struct ui_test_step {
    void (*func)(void* args);
    void* args;
    uint64_t serial;
    const bool* flag;
};

static struct {
    struct gpu_device* device;
    void (*const* cases)(void);
    uint32_t case_count;
    uint32_t case_index;
    void (*done)(void* user);
    void* user;
    uint64_t serial;
    const bool* flag;
    struct ui_test_step steps[ui_test_runner_steps];
    uint32_t head;
    uint32_t tail;
    bool running;
} ui_test_runner_;

void ui_test_await_(uint64_t serial) {
    ui_test_runner_.serial = max_uint64(ui_test_runner_.serial, serial);
}

void ui_test_await_flag_(const bool* flag) {
    assert(ui_test_runner_.flag == NULL);
    ui_test_runner_.flag = flag;
}

void ui_test_then_(void (*func)(void* args), void* args) {
    assert(ui_test_runner_.tail - ui_test_runner_.head < ui_test_runner_steps);
    ui_test_runner_.steps[ui_test_runner_.tail++ % ui_test_runner_steps] = (struct ui_test_step) {
        .func = func,
        .args = args,
        .serial = ui_test_runner_.serial,
        .flag = ui_test_runner_.flag,
    };
    ui_test_runner_.serial = 0;
    ui_test_runner_.flag = NULL;
}

static bool ui_test_pipelines_ready(void) {
    uint64_t serial = gpu_device_get_pipelines_serial(ui_test_runner_.device);
#if cr_macos || cr_ios
    gpu_device_wait(ui_test_runner_.device, serial);
    return true;
#else
    return gpu_device_is_completed(ui_test_runner_.device, serial);
#endif
}

static bool ui_test_step_ready(const struct ui_test_step* step) {
    bool flag = step->flag == NULL || *step->flag;
#if cr_macos || cr_ios
    assert(flag && "native tests load their data synchronously");
    gpu_device_wait(ui_test_runner_.device, step->serial);
    return true;
#else
    return flag && gpu_device_is_completed(ui_test_runner_.device, step->serial);
#endif
}

void ui_test_wake_(void) {
    if (ui_test_runner_.running || ui_test_runner_.done == NULL) {
        return;
    }
    ui_test_runner_.running = true;
    bool finished = false;
    while (finished == false) {
        if (ui_test_runner_.head != ui_test_runner_.tail) {
            struct ui_test_step step = ui_test_runner_.steps[ui_test_runner_.head % ui_test_runner_steps];
            if (ui_test_step_ready(&step) == false) {
                break;
            }
            assert(gpu_device_is_failed(ui_test_runner_.device, step.serial) == false);
            ui_test_runner_.head++;
            step.func(step.args);
        } else if (ui_test_runner_.case_index < ui_test_runner_.case_count) {
            if (ui_test_pipelines_ready() == false) {
                break;
            }
            ui_test_runner_.cases[ui_test_runner_.case_index++]();
        } else {
            finished = true;
        }
        assert(ui_test_runner_.serial == 0 && ui_test_runner_.flag == NULL && "ui_test_await_ needs a ui_test_then_");
    }
    ui_test_runner_.running = false;

    if (finished) {
        void (*done)(void* user) = ui_test_runner_.done;
        ui_test_runner_.done = NULL;
#if cr_wasm
        gpu_device_set_on_completed(ui_test_runner_.device, NULL, NULL);
#endif
        done(ui_test_runner_.user);
    }
}

#if cr_wasm
static void ui_test_runner_on_completed(void* user) {
    ui_test_wake_();
}
#endif

void ui_test_run_(struct gpu_device* device, void (*const* cases)(void), uint32_t case_count, void (*done)(void* user), void* user) {
    assert(ui_test_runner_.done == NULL);
    ui_test_runner_.device = device;
    ui_test_runner_.cases = cases;
    ui_test_runner_.case_count = case_count;
    ui_test_runner_.case_index = 0;
    ui_test_runner_.done = done;
    ui_test_runner_.user = user;
#if cr_wasm
    gpu_device_set_on_completed(device, ui_test_runner_on_completed, NULL);
#endif
    ui_test_wake_();
}
