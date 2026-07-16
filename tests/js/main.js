'use strict';

const js_tests = [];
const js_test_state = {
    group: '',
    passed: 0,
    failed: 0,
    app: 0,
    window: 0,
    device: 0,
    started: false,
    beginMs: 0,
    iterations: 1,
    waiter: null,
    fontData: null,
    font: 0,
};

function js_test(group, fn) {
    js_tests.push({ group: group, fn: fn });
}

function check(ok, label) {
    console.log('js ' + js_test_state.group + ' ' + label + ': ' + (ok ? 'ok' : 'FAIL'));
    if (ok) {
        js_test_state.passed++;
    } else {
        js_test_state.failed++;
    }
}

function near(a, b, epsilon = 1e-6) {
    return Math.abs(a - b) <= epsilon;
}

function eq(a, b, epsilon = 1e-6) {
    if (a === b) {
        return true;
    }
    if (typeof a === 'number' && typeof b === 'number') {
        return near(a, b, epsilon);
    }
    if (Array.isArray(a) && Array.isArray(b)) {
        return a.length === b.length && a.every((v, i) => eq(v, b[i], epsilon));
    }
    if (typeof a === 'object' && typeof b === 'object' && a !== null && b !== null) {
        const keys = Object.keys(b);
        return keys.every((k) => eq(a[k], b[k], epsilon));
    }
    return false;
}

function gpuWait(serial) {
    const device = js_test_state.device;
    if (gpu.deviceWait !== undefined) {
        gpu.deviceWait(device, serial);
        return undefined;
    }
    if (gpu.deviceIsCompleted(device, serial)) {
        return undefined;
    }
    return new Promise((resolve) => {
        js_test_state.waiter = { serial: serial, resolve: resolve };
    });
}

function js_test_on_completed() {
    const waiter = js_test_state.waiter;
    if (waiter !== null && gpu.deviceIsCompleted(js_test_state.device, waiter.serial)) {
        js_test_state.waiter = null;
        waiter.resolve();
    }
}

function touchEvent(type, id, pos, timestamp) {
    return { type: 'pointer', pointer: { type: type, id: id, pos: pos, timestamp: timestamp } };
}

function hoverEvent(type, id, pos, timestamp) {
    return { type: 'pointer', pointer: { type: type, source: 'indirect', id: id, pos: pos, timestamp: timestamp } };
}

function dispatch(root, timestamp, events) {
    ui.inputDispatch(root, timestamp, events, 1.0);
}

function touch(root, type, id, pos, timestamp) {
    dispatch(root, timestamp, [touchEvent(type, id, pos, timestamp)]);
}

async function js_test_run() {
    const state = js_test_state;
    state.beginMs = performance.now();
    for (const test of js_tests) {
        state.group = test.group;
        await test.fn();
    }
    const pass = state.failed === 0;
    console.log('finished in ' + ((performance.now() - state.beginMs) / 1000).toFixed(2) + 's');
    console.log('result: ' + (pass ? 'pass' : 'fail'));
    uniform.exit(pass ? 0 : 1);
}

function js_test_abort(error) {
    console.error(error);
    uniform.exit(1);
}

function js_test_on_update(window) {
    if (js_test_state.started === false) {
        js_test_state.started = true;
        js_test_run().catch(js_test_abort);
    }
}

function js_test_on_start(a) {
    const state = js_test_state;
    state.app = a;
    state.device = app.getDevice(a);
    if (gpu.deviceSetOnCompleted !== undefined) {
        gpu.deviceSetOnCompleted(state.device, js_test_on_completed);
    }
    for (let i = 1; i + 1 < uniform.args.length; i++) {
        if (uniform.args[i] === '--iterations') {
            state.iterations = Math.max(1, Number(uniform.args[i + 1]) | 0);
        }
    }
    ui.init(state.device);
    ui.inputSet(ui.inputCreate());
    state.font = ui.fontCreate(state.fontData);
    ui.setDefaultFont(state.font);
    state.window = app.windowCreate(a, {
        onUpdate: js_test_on_update,
        onCloseRequest: () => uniform.exit(1),
        title: 'uniform js tests',
        size: [640, 400],
        center: true,
    });
}

async function js_test_boot() {
    const response = await fetch('Inter.ttf');
    js_test_state.fontData = await response.arrayBuffer();
    app.run({ onStart: js_test_on_start });
}

js_test_boot().catch(js_test_abort);
