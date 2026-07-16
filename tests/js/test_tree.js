'use strict';

js_test('tree', async () => {
    const allocs = ui.getNodeAllocCount();
    const root = ui.frame({ size: ui.sizeFix(200, 100), layout: ui.layoutStackX('start', 'start', 10, ui.pad(5)) });
    const a = ui.frame({ parent: root, key: 'a', size: ui.sizeFix(30, 20), fill: ui.fillSolid(ui.red) });
    const b = ui.rect({ parent: root, size: ui.sizeFix(40, 20), corners: ui.corners(4) });
    const c = ui.ellipse({ parent: root, size: ui.sizeFix(20, 20), outline: ui.outlineSolid('inside', 1, ui.black) });
    const d = ui.line({ parent: root, size: ui.sizeFix(20, 20), thickness: 2, fill: ui.fillSolid(ui.black) });
    const inner = ui.frame({ parent: a, size: ui.sizeFix(10, 10), pos: ui.posXy(2, 3) });
    ui.layout(root);

    check(eq(ui.getSize(root), [200, 100]) && eq(ui.getSize(a), [30, 20]), 'sizes come back as vectors');
    check(eq(ui.getOriginFromRoot(root, a), [5, 5]) && eq(ui.getOriginFromRoot(root, b), [45, 5]) && eq(ui.getOriginFromRoot(root, c), [95, 5]) && eq(ui.getOriginFromRoot(root, d), [125, 5]), 'stack layout places children');
    check(eq(ui.getOriginFromRoot(root, inner), [7, 8]) && eq(ui.toLocalFromRoot(root, inner, [10, 10]), [3, 2]), 'manual positions nest');
    check(ui.getRoot(inner) === root && ui.getRoot(root) === root && ui.getRoot(ui.nodeNull) === ui.nodeNull, 'getRoot walks up');
    check(ui.isAlive(a) && ui.getKey(a) !== 0 && ui.getKey(a) !== ui.getKey(b) && ui.getKey(a) < 2 ** 53, 'nodes carry distinct keys');
    check(ui.deriveKey(root, 'a') === ui.deriveKey(root, 'a') && ui.deriveKey(root, 'a') !== ui.deriveKey(root, 'b') && ui.deriveKey(root, 'a') !== ui.deriveKey(a, 'a') && ui.deriveKey(root) !== ui.deriveKey(root), 'derived keys depend on the node and the key');

    const hits = ui.hitTest(root, [50, 10], () => true, 8);
    check(hits.length === 2 && hits[0].node === b && hits[0].contains && hits[1].node === root, 'hit test returns the chain from the target up');
    const seen = [];
    const filtered = ui.hitTest(root, [8, 9], (node) => { seen.push(node); return node === a; }, 8);
    check(filtered.length === 2 && filtered[0].node === a && seen.indexOf(inner) >= 0, 'hit test calls the predicate for every candidate');
    check(ui.hitTest(root, [300, 300], () => true, 8).length === 0 && ui.hitTest(ui.nodeNull, [0, 0], () => true, 8).length === 0, 'misses return an empty chain');

    ui.release(root);
    check(ui.isAlive(a) === false && ui.getNodeAllocCount() === allocs, 'release frees every node');

    const keyed = ui.frame({ key: 'root', size: ui.sizeFix(1, 1) });
    const child = ui.node(keyed, 123);
    const filled = ui.text({ node: child, str: ui.str('x', ui.black, 10) });
    const again = ui.frame({ key: 'root', size: ui.sizeFix(1, 1) });
    const childAgain = ui.node(again, ui.keyU64(123));
    ui.text({ node: childAgain, str: ui.str('x', ui.black, 10) });
    check(filled === child && ui.getKey(child) === ui.getKey(childAgain) && ui.getKey(keyed) === ui.getKey(again) && ui.getKey(child) !== ui.getKey(keyed), 'ui.node declares a node that content fills later and keys are stable across rebuilds');
    ui.release(keyed);
    ui.release(again);

    const stale = ui.frame({ size: ui.sizeFix(1, 1) });
    ui.release(stale);
    const fresh = ui.frame({ size: ui.sizeFix(1, 1) });
    check(ui.isAlive(stale) === false && ui.isAlive(fresh), 'handles of released nodes stay dead');
    ui.release(fresh);
});

js_test('text', async () => {
    const root = ui.frame({ size: ui.sizeFix(300, 100) });
    const single = ui.text({ parent: root, str: ui.str('hello world', ui.black, 13) });
    const wrapped = ui.text({ parent: root, size: { width: ui.sizeAxisFix(60) }, str: ui.str('one two three four', ui.black, 13) });
    const styled = ui.text({ parent: root, str: { raw: 'ab', attributes: [{ start: 0, end: 1, type: 'ptSize', ptSize: 30 }, { start: 1, end: 2, type: 'ptSize', ptSize: 10 }, { start: 0, end: 2, type: 'color', color: ui.red }] } });
    ui.layout(root);

    check(ui.textGetLineCount(single) === 1 && ui.textGetLineCount(wrapped) > 1, 'text wraps to its width');
    const line = ui.textGetLine(single, 0);
    check(line.start === 0 && line.end === 11 && line.width > 40 && line.height > 10 && line.y === 0, 'lines report their range and metrics');
    const x5 = ui.textGetX(single, 0, 5);
    check(x5 > 0 && x5 < line.width && ui.textGetOffset(single, 0, x5 + 1) === 5, 'x positions map to offsets');
    check(ui.textNextCluster(single, 0) === 1 && ui.textPrevCluster(single, 5) === 4, 'cluster stepping');
    const styledLine = ui.textGetLine(styled, 0);
    check(styledLine.height > ui.textGetLine(single, 0).height && ui.getSize(styled)[0] > 0, 'per range attributes apply');
    ui.release(root);

    const emoji = ui.frame({ size: ui.sizeFix(300, 100) });
    const text = ui.text({ parent: emoji, str: ui.str('aé€', ui.black, 13) });
    ui.layout(emoji);
    check(ui.textGetLine(text, 0).end === 6 && ui.textNextCluster(text, 1) === 3 && ui.textNextCluster(text, 3) === 6, 'offsets count utf8 bytes');
    ui.release(emoji);

    const bytes = new Uint8Array(await (await fetch('Inter.ttf')).arrayBuffer());
    const font = ui.fontCreate(bytes);
    check(font !== 0 && font !== js_test_state.font, 'fonts load from bytes');
    const custom = ui.frame({ size: ui.sizeFix(300, 100) });
    const a = ui.text({ parent: custom, str: ui.str('width', ui.black, 13) });
    const b = ui.text({ parent: custom, str: { raw: 'width', attributes: [{ start: 0, end: 5, type: 'ptSize', ptSize: 13 }, { start: 0, end: 5, type: 'font', font: font }] } });
    ui.layout(custom);
    check(near(ui.getSize(a)[0], ui.getSize(b)[0], 0.01), 'a font attribute selects the font');
    ui.release(custom);
    ui.fontDestroy(font);
});

js_test('path', async () => {
    const red = (pixel) => eq(pixel, js_test_red, 2);
    const black = (pixel) => eq(pixel, js_test_black, 2);
    const dark = (pixel) => pixel[2] < 128;
    const render = async (path, extra) => {
        const root = ui.frame({ size: ui.sizeFix(16, 16), fill: ui.fillSolid(ui.black) });
        ui.shape(Object.assign({ parent: root, pos: ui.posXy(0, 0), size: ui.sizeFix(16, 16), path: path, fill: ui.fillSolid(ui.red) }, extra));
        ui.pathDestroy(path);
        const pixel = await js_test_render(root, 16, 16);
        ui.release(root);
        return pixel;
    };

    const triangle = ui.pathCreate();
    ui.pathMoveTo(triangle, [0, 0]);
    ui.pathLineTo(triangle, [16, 0]);
    ui.pathLineTo(triangle, [0, 16]);
    ui.pathClose(triangle);
    const root = ui.frame({ size: ui.sizeFix(16, 16), fill: ui.fillSolid(ui.black) });
    const shape = ui.shape({ parent: root, pos: ui.posXy(0, 0), size: ui.sizeFix(16, 16), path: triangle, fill: ui.fillSolid(ui.red) });
    ui.pathDestroy(triangle);
    check(ui.isAlive(shape) && ui.getRoot(shape) === root, 'shapes take a path');
    let pixel = await js_test_render(root, 16, 16);
    ui.release(root);
    check(red(pixel(2, 2)) && red(pixel(6, 6)) && black(pixel(13, 13)) && black(pixel(14, 4)), 'nodes keep their own copy of the path');

    const bump = ui.pathCreate();
    ui.pathMoveTo(bump, [0, 16]);
    ui.pathQuadTo(bump, [8, -16], [16, 16]);
    ui.pathClose(bump);
    pixel = await render(bump);
    check(red(pixel(8, 2)) && red(pixel(4, 7)) && black(pixel(2, 1)) && black(pixel(1, 1)), 'quad curves lift to cubics');

    const stroke = ui.pathCreate();
    ui.pathMoveTo(stroke, [0, 8]);
    ui.pathLineTo(stroke, [16, 8]);
    pixel = await render(stroke, { mode: 'stroke', thickness: 6 });
    check(red(pixel(8, 8)) && black(pixel(8, 2)) && black(pixel(8, 13)), 'stroked paths follow the line');

    const circle = ui.pathCreate();
    ui.pathArc(circle, [8, 8], 6, 0, 2 * Math.PI, false);
    ui.pathClose(circle);
    pixel = await render(circle);
    check(red(pixel(8, 8)) && red(pixel(8, 4)) && black(pixel(1, 1)) && black(pixel(14, 14)), 'arcs sweep around their center');

    const d = ui.pathCreate();
    ui.pathMoveTo(d, [2, 2]);
    ui.pathLineTo(d, [8, 2]);
    ui.pathArc(d, [8, 8], 6, -Math.PI / 2, Math.PI / 2, false);
    ui.pathLineTo(d, [2, 14]);
    ui.pathClose(d);
    pixel = await render(d);
    check(red(pixel(4, 8)) && red(pixel(11, 8)) && black(pixel(13, 1)) && black(pixel(13, 14)), 'arcs continue an open subpath');

    const rounded = ui.pathCreate();
    ui.pathRoundedRect(rounded, [0, 0], [16, 16], ui.corners(6));
    pixel = await render(rounded);
    check(red(pixel(8, 8)) && red(pixel(8, 2)) && red(pixel(2, 8)) && dark(pixel(0, 0)) && dark(pixel(15, 15)), 'rounded rects round their corners');

    const ellipse = ui.pathCreate();
    ui.pathEllipse(ellipse, [0, 0], [16, 16]);
    pixel = await render(ellipse);
    check(red(pixel(8, 8)) && red(pixel(8, 2)) && black(pixel(0, 0)) && black(pixel(15, 0)), 'ellipses fill their bounds');

    const corners = ui.pathCreate();
    ui.pathMoveTo(corners, [0, 0]);
    ui.pathCorners(corners, [[16, 0], [16, 16], [0, 16], [0, 0]], [8, 0, 0, 0], 'continuous');
    pixel = await render(corners);
    check(red(pixel(8, 8)) && red(pixel(1, 1)) && red(pixel(14, 14)) && dark(pixel(15, 0)), 'corner paths round the listed points');
});

js_test('graphics', async () => {
    const pixels = new Uint8Array([0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255]);
    const graphic = ui.graphicCreate(2, 2, 'bgra8', pixels, 8, true);
    check(graphic !== 0, 'graphics come from pixel bytes');
    ui.graphicDestroy(graphic);

    const texture = gpu.textureCreate(js_test_state.device, { label: 'graphic', format: 'bgra8', size: [2, 2], usage: gpu.textureUsageSample | gpu.textureUsageUpload });
    check(gpu.textureGetFormat(texture) === 'bgra8' && eq(gpu.textureGetSize(texture), [2, 2]) && gpu.textureGetUsage(texture) === (gpu.textureUsageSample | gpu.textureUsageUpload), 'texture descriptors round trip');
    gpu.textureWrite(js_test_state.device, texture, 0, 0, 2, 2, pixels, 8);
    const fromTexture = ui.graphicCreateTexture(texture);
    check(fromTexture !== 0, 'graphics wrap textures and own them');
    ui.graphicDestroy(fromTexture);
});
