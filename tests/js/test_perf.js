'use strict';

function js_perf_on_tap() {}

function js_perf_scene(rows, cols) {
    const root = ui.frame({ size: ui.sizeFix(1024, 1024), layout: ui.layoutStackY('start', 'start', 4, ui.pad(8)), fill: ui.fillSolid(ui.gray10) });
    for (let r = 0; r < rows; r++) {
        const row = ui.frame({ parent: root, key: r, layout: ui.layoutStackX('start', 'center', 4, 0) });
        for (let c = 0; c < cols; c++) {
            const cell = ui.frame({ parent: row, key: c, size: ui.sizeFix(36, 24), fill: ui.fillSolid(ui.gray50), corners: ui.corners(4) });
            ui.tap({ node: cell, onChange: js_perf_on_tap });
            ui.text({ parent: cell, str: ui.str('ab', ui.white, 12) });
        }
    }
    return root;
}

function js_perf_format(samples) {
    const sorted = samples.slice().sort((a, b) => a - b);
    return sorted[0].toFixed(3) + '/' + sorted[sorted.length >> 1].toFixed(3);
}

function js_perf_time(label, count, fn, reset) {
    const runs = js_test_state.iterations;
    const samples = [];
    for (let run = 0; run < runs; run++) {
        const begin = performance.now();
        for (let i = 0; i < count; i++) {
            fn(i);
        }
        samples.push((performance.now() - begin) * 1000 / count);
        if (reset !== undefined) {
            reset();
        }
    }
    if (runs > 1) {
        console.log('js perf ' + label + ': ' + js_perf_format(samples) + ' us (min/median over ' + runs + ' runs of ' + count + ')');
    }
}

js_test('perf', async () => {
    ui.setToPxFromPt(1);
    const device = js_test_state.device;
    await gpuWait(gpu.deviceGetPipelinesSerial(device));
    const iterations = js_test_state.iterations;
    const warmup = iterations > 1 ? 10 : 0;
    const texture = gpu.textureCreate(device, { label: 'perf', format: 'bgra8', size: [1024, 1024], usage: gpu.textureUsageRenderTarget | gpu.textureUsageSample });
    const rows = 20;
    const cols = 25;
    const build = [];
    const layout = [];
    const render = [];
    const release = [];
    let prev = ui.nodeNull;
    for (let i = 0; i < warmup + iterations; i++) {
        dispatch(prev, 1 + i * 0.016, []);
        const t0 = performance.now();
        const root = js_perf_scene(rows, cols);
        const t1 = performance.now();
        ui.layout(root);
        const t2 = performance.now();
        const commandBuffer = gpu.commandBufferCreate(device, 'perf');
        ui.render(root, commandBuffer, texture, {});
        const serial = gpu.commandBufferCommit(commandBuffer);
        const t3 = performance.now();
        ui.release(prev);
        const t4 = performance.now();
        prev = root;
        if (i >= warmup) {
            build.push(t1 - t0);
            layout.push(t2 - t1);
            render.push(t3 - t2);
            release.push(t4 - t3);
        }
        await gpuWait(serial);
    }
    ui.release(prev);
    dispatch(ui.nodeNull, 100, []);
    gpu.textureDestroy(device, texture);
    const nodes = 1 + rows + rows * cols * 2;
    if (iterations > 1) {
        console.log('js perf scene ' + nodes + ' nodes, ' + rows * cols + ' gestures: build ' + js_perf_format(build) + ', layout ' + js_perf_format(layout) + ', render ' + js_perf_format(render) + ', release ' + js_perf_format(release) + ' ms (min/median over ' + iterations + ' frames)');
    }

    let root = ui.nodeNull;
    let child = ui.nodeNull;
    const cells = [];
    function reset() {
        ui.release(root);
        dispatch(ui.nodeNull, 200, []);
        root = ui.frame({ size: ui.sizeFix(100, 100) });
        child = ui.frame({ parent: root, size: ui.sizeFix(10, 10) });
        cells.length = 0;
        for (let i = 0; i < 2000; i++) {
            cells.push(ui.frame({ parent: root, size: ui.sizeFix(1, 1) }));
        }
        ui.layout(root);
    }
    reset();
    const calls = iterations > 1 ? 100000 : 100;
    const nodes_per_run = iterations > 1 ? 2000 : 20;
    js_perf_time('isAlive', calls, () => ui.isAlive(child));
    js_perf_time('getSize', calls, () => ui.getSize(child));
    js_perf_time('deriveKey', calls, () => ui.deriveKey(root, 'abc'));
    js_perf_time('frame', nodes_per_run, () => ui.frame({ parent: root, size: ui.sizeFix(10, 10), fill: ui.fillSolid(ui.red), corners: ui.corners(4) }), reset);
    js_perf_time('text', nodes_per_run, () => ui.text({ parent: root, str: ui.str('ab', ui.white, 12) }), reset);
    js_perf_time('tap', nodes_per_run, (i) => ui.tap({ node: cells[i], onChange: js_perf_on_tap }), reset);
    ui.release(root);
    dispatch(ui.nodeNull, 300, []);
    check(build.length === iterations, 'the perf scene builds, lays out and renders');
});
