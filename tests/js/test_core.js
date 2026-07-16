'use strict';

js_test('host', async () => {
    check(uniform.args.length >= 1 && uniform.args[0].endsWith('main.js'), 'args start with the script path');

    const bytes = new TextEncoder().encode('héllo €');
    check(bytes.length === 10 && bytes[1] === 0xc3, 'TextEncoder produces utf8');
    check(new TextDecoder().decode(bytes) === 'héllo €', 'TextDecoder reads utf8');
    check(new TextDecoder().decode(new Uint8Array([0x61, 0xff, 0x62])) === 'a�b', 'TextDecoder replaces invalid bytes');

    let timers = '';
    await new Promise((resolve) => {
        setTimeout(() => {
            timers += 'b';
            resolve();
        }, 0);
        timers += 'a';
    });
    check(timers === 'ab', 'setTimeout runs after the current task');

    let ticks = 0;
    await new Promise((resolve) => {
        const id = setInterval(() => {
            if (++ticks === 3) {
                clearInterval(id);
                resolve();
            }
        }, 0);
    });
    await new Promise((resolve) => setTimeout(resolve, 0));
    check(ticks === 3, 'setInterval repeats until cleared');

    let order = '';
    queueMicrotask(() => { order += 'b'; });
    order += 'a';
    await Promise.resolve();
    check(order === 'ab', 'queueMicrotask runs after the current task');

    const response = await fetch('main.js');
    const text = await response.text();
    check(response.ok && response.status === 200 && text.indexOf('js_test_run') >= 0, 'fetch resolves relative to the script');
    const missing = await fetch('does-not-exist.txt');
    check(missing.ok === false, 'fetch reports a missing file');
    const font = new Uint8Array(await (await fetch('Inter.ttf')).arrayBuffer());
    check(font.length > 100000 && font[0] === 0 && font[1] === 1, 'fetch returns binary data');
});

js_test('memory', async () => {
    const ptr = uniform.alloc(16);
    check(ptr !== 0 && ptr % 16 === 0, 'alloc returns an aligned pointer');
    uniform.write(ptr, new Uint8Array([1, 2, 3, 4]));
    uniform.write(ptr + 4, new Float32Array([1.5]).buffer);
    const back = uniform.read(ptr, 8);
    check(back[0] === 1 && back[3] === 4 && new Float32Array(back.buffer, 4, 1)[0] === 1.5, 'write and read move bytes');
    uniform.free(ptr);

    const big = uniform.alloc(3 << 20);
    const data = new Uint8Array(3 << 20);
    data[data.length - 1] = 7;
    uniform.write(big, data);
    check(uniform.read(big + data.length - 1, 1)[0] === 7, 'large writes reach the end of the block');
    uniform.free(big);

    check(uniform.sizeof('ui_color') === 32 && uniform.sizeof('ui_key') === 8, 'sizeof reports struct sizes');
});

js_test('structs', async () => {
    const color = uniform.alloc(uniform.sizeof('ui_color'));
    uniform.writeStruct('ui_color', color, { value: [0.25, 0.5, 0.75, 1], colorSpace: 'displayP3' });
    const colorBack = uniform.readStruct('ui_color', color);
    check(eq(colorBack.value, [0.25, 0.5, 0.75, 1]) && colorBack.colorSpace === 'displayP3', 'vectors and enums round trip');
    uniform.writeStruct('ui_color', color, { value: [1, 1, 1, 1], colorSpace: 2 });
    check(uniform.readStruct('ui_color', color).colorSpace === 'displayP3', 'enums accept numbers');
    uniform.free(color);

    const frame = uniform.alloc(uniform.sizeof('ui_frame'));
    const node = ui.nodeMake(3, 9);
    uniform.writeStruct('ui_frame', frame, {
        node: node,
        key: 'frame key',
        pos: ui.posXy(1.5, -2.5),
        size: ui.sizeFix(100, 50),
        transform: [1, 2, 3, 4, 5, 6],
        layout: ui.layoutStackY('center', 'end', 7, ui.padXy(1, 2)),
        fill: { layers: [ui.layerSolid(ui.red), ui.layerOpacity(ui.layerGradient('up', ui.blue, ui.green), 0.5)] },
        outline: ui.outlineSolid('outside', 3, ui.black),
        corners: ui.cornersCircular(8),
        shapeGroup: { op: 'subtract', smoothing: 2 },
        shadow: ui.shadow(1, 2, 3, ui.gray50),
        blur: ui.blur(4),
        clip: true,
        backdropRoot: false,
        blend: { mode: 'normal', opacity: 0.25 },
    });
    const f = uniform.readStruct('ui_frame', frame);
    uniform.free(frame);
    check(f.node === node && ui.nodeGetGen(f.node) === 3 && ui.nodeGetIdx(f.node) === 9, 'handles round trip');
    check(f.key === ui.key('frame key') && f.key !== undefined, 'string keys are hashed');
    check(f.pos.x.mode === 'rel' && f.pos.x.anchor === 'start' && f.pos.x.parentAnchor === 'start' && f.pos.x.constant === 1.5 && f.pos.y.constant === -2.5, 'nested records round trip');
    check(f.size.width.mode === 'fix' && f.size.width.constant === 100 && f.size.height.constant === 50, 'size helper fields');
    check(eq(f.transform, [1, 2, 3, 4, 5, 6]), 'matrices round trip');
    check(f.layout.mode === 'stackY' && f.layout.dist === 'center' && f.layout.align === 'end' && f.layout.spacing === 7 && eq(f.layout.padding, [1, 1, 2, 2]) && f.layout.paddingTop === 2, 'unions with vectors round trip');
    check(f.fill.layers.length === 4 && f.fill.layers[0].type === 'solid' && eq(f.fill.layers[0].solid.color.value, [1, 0, 0, 1]), 'fixed arrays of records round trip');
    check(f.fill.layers[1].type === 'gradient' && f.fill.layers[1].gradient.direction === 'up' && f.fill.layers[1].blend.mode === 'normal' && f.fill.layers[1].blend.opacity === 0.5 && f.fill.layers[2].type === 'none', 'discriminated unions round trip');
    check(f.outline.align === 'outside' && f.outline.thickness === 3 && f.outline.layers[0].type === 'solid', 'outline round trips');
    check(f.corners.profile === 'circular' && eq(f.corners.radius, [8, 8, 8, 8]), 'corners round trip');
    check(f.shapeGroup.op === 'subtract' && f.shapeGroup.smoothing === 2 && f.shadow.radius === 3 && eq(f.shadow.offset, [1, 2]) && f.blur.radius === 4, 'scalars round trip');
    check(f.clip === true && f.backdropRoot === false && f.blend.mode === 'normal' && f.blend.opacity === 0.25, 'bools round trip');

    const event = uniform.alloc(uniform.sizeof('app_event'));
    uniform.writeStruct('app_event', event, { type: 'pointer', pointer: { type: 'touchMoved', source: 'pencil', button: 'middle', id: 2 ** 40 + 5, modifiers: 6, pressure: 0.5, radius: 3, pos: [10, 20], timestamp: 1.25 } });
    const e = uniform.readStruct('app_event', event);
    check(e.type === 'pointer' && e.pointer.type === 'touchMoved' && e.pointer.source === 'pencil' && e.pointer.button === 'middle' && e.pointer.id === 2 ** 40 + 5 && e.pointer.modifiers === 6 && e.pointer.pressure === 0.5 && eq(e.pointer.pos, [10, 20]) && e.pointer.timestamp === 1.25, 'pointer events round trip with 64 bit ids');
    uniform.writeStruct('app_event', event, { type: 'key', key: { key: 'f12', modifiers: 1, down: true, repeat: false } });
    const k = uniform.readStruct('app_event', event);
    check(k.type === 'key' && k.key.key === 'f12' && k.key.down === true && k.key.repeat === false && k.pointer === undefined, 'union members outside the active variant are left out');
    uniform.free(event);

    const profile = uniform.alloc(uniform.sizeof('gpu_profile'));
    uniform.writeStruct('gpu_profile', profile, { passCount: 2, span: 1.5, passes: [0.25, 0.5], passNames: ['first', 'second pass'] });
    const p = uniform.readStruct('gpu_profile', profile);
    check(p.passCount === 2 && p.span === 1.5 && p.passes.length === gpu.profileMaxPasses && p.passes[1] === 0.5 && p.passes[2] === 0, 'fixed scalar arrays round trip');
    check(p.passNames.length === gpu.profileMaxPasses && p.passNames[0] === 'first' && p.passNames[1] === 'second pass' && p.passNames[2] === '', 'fixed char arrays round trip as strings');
    uniform.free(profile);

    const desc = uniform.alloc(uniform.sizeof('ui_gesture_desc'));
    uniform.writeStruct('ui_gesture_desc', desc, { node: 7, stateSize: 2 ** 33 + 1 });
    const d = uniform.readStruct('ui_gesture_desc', desc);
    const handler = uniform.alloc(uniform.sizeof('ui_on_pointer'));
    uniform.writeStruct('ui_on_pointer', handler, { node: 7, user: ui.inputGet() });
    const h = uniform.readStruct('ui_on_pointer', handler);
    check(d.node === 7 && d.stateSize === 2 ** 33 + 1 && d.create === 0 && h.user === ui.inputGet(), 'pointers and 64 bit integers round trip');
    uniform.free(handler);
    uniform.free(desc);
});

js_test('values', async () => {
    check(ui.key('a') === ui.keyStr('a') && ui.key('a') !== ui.key('b'), 'ui.key hashes strings like keyStr');
    check(ui.key(7) === ui.keyU64(7) && ui.key(7) !== 7, 'ui.key packs numbers like keyU64');
    check(ui.key(ui.key(9)) === ui.key(9) && ui.key(undefined) === undefined, 'ui.key passes keys through');
    check(ui.keyU64(2 ** 52) === ui.keyU64(0) && ui.keyU64(0) !== 0 && ui.keyU64(2 ** 53 - 1) < 2 ** 53, 'keys pack 52 bits and the explicit flag into 53 bits');
    check(ui.keyAuto === undefined && ui.nodeNull === 0 && ui.pointerNull === 0, 'null values are zero or absent');

    const node = ui.nodeMake(5, 1234);
    check(ui.nodeGetGen(node) === 5 && ui.nodeGetIdx(node) === 1234 && ui.nodeIsNull(node) === false && ui.nodeIsNull(ui.nodeNull), 'node handles pack a generation and an index');
    check(ui.nodeIsEqual(node, ui.nodeMake(5, 1234)) && ui.nodeIsEqual(node, ui.nodeMake(6, 1234)) === false, 'node handles compare by value');
    check(ui.pointerGetIdx(ui.pointerMake(1, 2)) === 2 && ui.pointerIsNull(ui.pointerNull), 'pointer handles work the same way');

    check(eq(ui.sizeFix(3, 4), { width: { mode: 'fix', constant: 3 }, height: { mode: 'fix', constant: 4 } }) && ui.sizeFill().width.mode === 'fill' && ui.sizeAxisWidth().multiplier === 1, 'size helpers');
    check(eq(ui.posXy(1, 2), { x: { mode: 'rel', anchor: 'start', parentAnchor: 'start', constant: 1 }, y: { mode: 'rel', anchor: 'start', parentAnchor: 'start', constant: 2 } }) && ui.posCenter().y.anchor === 'center' && ui.posAxisAuto().mode === 'auto', 'pos helpers');
    check(eq(ui.layoutStackX('center', 'end', 3, 4).padding, [4, 4, 4, 4]) && eq(ui.layoutStackY('start', 'start', 0, ui.padXy(1, 2)).padding, [1, 1, 2, 2]), 'layout helpers broadcast padding');
    check(eq(ui.hex(0xff8000).value, [1, 128 / 255, 0, 1]) && ui.hex(0xff8000).colorSpace === 'srgb' && ui.hexA(0x0000ff, 0.5).value[3] === 0.5, 'hex colors');
    check(eq(ui.white.value, [1, 1, 1, 1]) && eq(ui.clear.value, [0, 0, 0, 0]) && eq(ui.gray(0.3).value, [0.3, 0.3, 0.3, 1]) && eq(ui.rgbaCs(1, 2, 3, 4, 'srgbLinear'), { value: [1, 2, 3, 4], colorSpace: 'srgbLinear' }), 'color constants and helpers');
    check(ui.fillSolid(ui.red).layers[0].type === 'solid' && ui.fillGradient('down', ui.red, ui.blue).layers[0].gradient.direction === 'down' && ui.layerOpacity(ui.layerSolid(ui.red), 0.5).blend.opacity === 0.5, 'layer helpers');
    check(ui.outlineSolid('center', 2, ui.red).thickness === 2 && ui.shadow(1, 2, 3, ui.red).offset[1] === 2 && ui.corners(5).radius[3] === 5 && ui.cornersCircular(5).profile === 'circular', 'outline, shadow and corner helpers');
    const str = ui.strWeight('text', ui.red, 12, 600);
    check(str.raw === 'text' && str.attributes.length === 3 && str.attributes[1].type === 'ptSize' && str.attributes[1].ptSize === 12 && str.attributes[2].weight === 600 && str.attributes[0].end === 4294967295, 'string helpers set attributes');
    check(ui.colorSpaceFromApp('displayP3') === 'displayP3' && ui.colorSpaceFromApp('srgb') === 'srgb' && ui.pixelFormatFromApp('rgba16f') === 'rgba16f' && ui.pixelFormatFromApp('bgra8') === 'bgra8', 'app conversions');
    check(eq(hud.theme(true).blue.value, ui.hex(0x0076ff).value) && hud.theme(false).font === 0, 'hud theme helper');
    check(app.deltaMax === 0.1 && gpu.textureUsageRead === 16 && gpu.bufferUsageStorage === 2 && ui.layerMax === 4 && ui.pathCornersMax === 256 && ui.glassSliderDefault === 0.5, 'constants');
    check(app.modifierShift === 1 && app.modifierCommand === 8, 'flag enums are numeric constants');
});

js_test('errors', async () => {
    const root = ui.frame({ size: ui.sizeFix(20, 20) });
    ui.layout(root);
    let calls = 0;
    const hits = ui.hitTest(root, [10, 10], () => {
        calls++;
        throw new Error('expected: a callback throws');
    }, 8);
    check(calls === 1 && hits.length === 0, 'a callback that throws is reported and returns nothing');
    ui.release(root);

    let later = false;
    await new Promise((resolve) => {
        setTimeout(() => {
            throw new Error('expected: a timer throws');
        }, 0);
        setTimeout(() => {
            later = true;
            resolve();
        }, 0);
    });
    check(later, 'a timer that throws is reported and later timers still run');
});

js_test('checks', async () => {
    const state = js_test_state;
    const device = state.device;
    const allocs = ui.getNodeAllocCount();

    const stale = ui.frame({ size: ui.sizeFix(1, 1) });
    ui.release(stale);
    let error = null;
    try {
        ui.release(stale);
    } catch (e) {
        error = e;
    }
    check(error instanceof Error && error.check === true && error.message.indexOf('is not alive') >= 0, 'a stale handle throws a check error');

    const roots = [];
    error = null;
    try {
        for (let i = 0; i < 5000; i++) {
            roots.push(ui.frame({ size: ui.sizeFix(1, 1) }));
        }
    } catch (e) {
        error = e;
    }
    const counted = error === null ? null : /(\d+) of them roots/.exec(error.message);
    check(error !== null && error.check === true && error.message.indexOf('node pool is exhausted') >= 0 && counted !== null && Number(counted[1]) >= roots.length, 'exhausting the node pool names the leaked roots');
    for (const root of roots) {
        ui.release(root);
    }
    const again = ui.frame({ size: ui.sizeFix(1, 1) });
    check(ui.isAlive(again) && ui.getNodeAllocCount() === allocs + 1, 'the context stays usable after a check');
    ui.release(again);

    error = null;
    try {
        app.windowAcquireTarget(state.window);
    } catch (e) {
        error = e;
    }
    check(error !== null && error.check === true && error.message.indexOf('on_update') >= 0, 'acquiring a target outside of an update throws');

    const texture = gpu.textureCreate(device, { label: 'check target', format: 'bgra8', size: [4, 4], usage: gpu.textureUsageRenderTarget });
    const commandBuffer = gpu.commandBufferCreate(device, 'check');
    const serial = gpu.commandBufferGetSerial(commandBuffer);
    gpu.commandBufferBeginRender(commandBuffer, texture, 'clear', 'check pass');
    error = null;
    try {
        gpu.commandBufferCommit(commandBuffer);
    } catch (e) {
        error = e;
    }
    check(error !== null && error.check === true && error.message.indexOf('pass') >= 0, 'committing with an active pass throws');
    gpu.commandBufferDiscard(commandBuffer);
    check(gpu.deviceIsCompleted(device, serial), 'a discarded command buffer completes');

    let abandoned = 0;
    await new Promise((resolve) => {
        setTimeout(() => {
            abandoned = gpu.commandBufferGetSerial(gpu.commandBufferCreate(device, 'abandoned'));
            throw new Error('expected: a timer leaves a command buffer behind');
        }, 0);
        setTimeout(resolve, 0);
    });
    check(abandoned !== 0 && gpu.deviceIsCompleted(device, abandoned), 'an uncaught error discards the command buffers of its dispatch');
    gpu.textureDestroy(device, texture);

    const size = app.windowGetSize(state.window);
    const native = app.windowGetNative(state.window);
    let starts = 0;
    const desc = {
        onStart: (a) => {
            starts++;
            js_test_on_start(a);
        },
        onQuit: () => {
            ui.inputDestroy(ui.inputGet());
            ui.deinit();
        },
    };
    ui.inputDestroy(ui.inputGet());
    ui.deinit();
    app.restart(state.app, desc);
    check(starts === 1, 'the test app restarts with a quitting description');
    const gone = ui.frame({ size: ui.sizeFix(1, 1) });
    ui.release(gone);
    setTimeout(() => ui.release(gone), 0);
    for (let i = 0; i < 100 && starts < 2; i++) {
        await new Promise((resolve) => setTimeout(resolve, 20));
    }
    check(starts === 2, 'an uncaught check error restarts the app');
    check(state.window !== 0 && eq(app.windowGetSize(state.window), size), 'the restarted window takes the place of the previous one');
    check(typeof document !== 'undefined' || app.windowGetNative(state.window) === native, 'the restarted window keeps its native window');
});
