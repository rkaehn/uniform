'use strict';

js_test('tree', async () => {
    ui.setToPxFromPt(1);
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
    ui.setToPxFromPt(1);
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
    const size = uniform.sizeof('ui_path_component');
    const components = (p) => {
        const base = ui.pathGetComponents(p);
        return Array.from({ length: ui.pathGetCount(p) }, (_, i) => uniform.readStruct('ui_path_component', base + i * size));
    };

    const path = ui.pathCreate();
    ui.pathMoveTo(path, [0, 0]);
    ui.pathLineTo(path, [10, 0]);
    ui.pathQuadTo(path, [15, 5], [10, 10]);
    ui.pathClose(path);
    const built = components(path);
    check(built.length === 4 && built[0].type === 'move' && built[2].type === 'cubic' && built[3].type === 'close', 'paths record components');
    check(eq(built[2].cubic.c1, [40 / 3, 10 / 3]) && eq(built[2].cubic.c2, [40 / 3, 20 / 3]) && eq(built[2].cubic.p, [10, 10]), 'quadTo lifts to a cubic');

    ui.pathAppend(path, [{ type: 'line', line: { p: [3, 4] } }]);
    const appended = components(path);
    check(appended.length === 5 && appended[4].type === 'line' && eq(appended[4].line.p, [3, 4]), 'pathAppend takes component objects');

    const root = ui.frame({ size: ui.sizeFix(20, 20) });
    const shape = ui.shape({ parent: root, size: ui.sizeFix(20, 20), mode: 'stroke', thickness: 1, path: path, fill: ui.fillSolid(ui.black) });
    ui.layout(root);
    check(ui.isAlive(shape) && ui.getRoot(shape) === root, 'shapes take a path');
    ui.release(root);
    ui.pathDestroy(path);

    const arc = ui.pathCreate();
    ui.pathArc(arc, [10, 10], 5, 0, Math.PI, false);
    const arcs = components(arc);
    check(arcs.length >= 2 && arcs[0].type === 'move' && near(arcs[0].move.p[0], 15) && near(arcs[arcs.length - 1].cubic.p[0], 5, 1e-9), 'arcs start at the angle');
    ui.pathDestroy(arc);

    const rounded = ui.pathCreate();
    ui.pathRoundedRect(rounded, [0, 0], [20, 10], ui.corners(2));
    const roundeds = components(rounded);
    check(roundeds.length > 4 && roundeds[roundeds.length - 1].type === 'close', 'rounded rects close');
    ui.pathDestroy(rounded);

    const ellipse = ui.pathCreate();
    ui.pathEllipse(ellipse, [0, 0], [20, 10]);
    check(components(ellipse).filter((c) => c.type === 'cubic').length === 4, 'ellipses use four cubics');
    ui.pathDestroy(ellipse);

    const corners = ui.pathCreate();
    ui.pathCorners(corners, [[0, 0], [20, 0], [20, 20]], [0, 4, 0], true);
    const cornered = components(corners);
    check(cornered.length > 3 && cornered.some((c) => c.type === 'cubic'), 'corner paths round the listed points');
    ui.pathDestroy(corners);

    const chained = ui.pathCreate();
    ui.pathMoveTo(chained, [0, 0]);
    ui.pathLineTo(chained, [10, 0]);
    ui.pathArc(chained, [10, 5], 5, -Math.PI / 2, Math.PI / 2, true);
    const chain = components(chained);
    check(chain.length > 3 && chain[1].type === 'line', 'arcs continue an open subpath');
    ui.pathDestroy(chained);
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
