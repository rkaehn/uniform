'use strict';

async function js_test_render(root, width, height, options) {
    const device = js_test_state.device;
    await gpuWait(gpu.deviceGetPipelinesSerial(device));
    const texture = gpu.textureCreate(device, { label: 'js test target', format: 'bgra8', size: [width, height], usage: gpu.textureUsageRenderTarget | gpu.textureUsageSample | gpu.textureUsageRead });
    const out = uniform.alloc(width * height * 4);
    const commandBuffer = gpu.commandBufferCreate(device, 'js test');
    ui.setToPxFromPt(1);
    ui.layout(root);
    ui.render(root, commandBuffer, texture, options === undefined ? {} : options);
    gpu.commandBufferReadTexture(commandBuffer, texture, out, width * 4);
    const serial = gpu.commandBufferCommit(commandBuffer);
    await gpuWait(serial);
    const pixels = uniform.read(out, width * height * 4);
    uniform.free(out);
    gpu.textureDestroy(device, texture);
    return (x, y) => Array.from(pixels.subarray((y * width + x) * 4, (y * width + x) * 4 + 4));
}

const js_test_red = [0, 0, 255, 255];
const js_test_green = [0, 255, 0, 255];
const js_test_blue = [255, 0, 0, 255];
const js_test_white = [255, 255, 255, 255];
const js_test_black = [0, 0, 0, 255];

js_test('render', async () => {
    const solid = ui.frame({ size: ui.sizeFix(4, 4), fill: ui.fillSolid(ui.red) });
    let pixel = await js_test_render(solid, 4, 4);
    check(eq(pixel(0, 0), js_test_red) && eq(pixel(3, 3), js_test_red), 'a solid fill covers the target');
    ui.release(solid);

    const row = ui.frame({ size: ui.sizeFix(8, 4), layout: ui.layoutStackX('start', 'start', 0, 0), fill: ui.fillSolid(ui.black) });
    ui.frame({ parent: row, size: ui.sizeFix(4, 4), fill: ui.fillSolid(ui.red) });
    ui.rect({ parent: row, size: ui.sizeFix(4, 4), fill: ui.fillSolid(ui.blue) });
    pixel = await js_test_render(row, 8, 4);
    check(eq(pixel(1, 1), js_test_red) && eq(pixel(6, 2), js_test_blue), 'children render where the layout puts them');
    ui.release(row);

    const options = ui.frame({ size: ui.sizeFix(4, 4), fill: ui.fillSolid(ui.green) });
    pixel = await js_test_render(options, 4, 4, { outputColorSpace: 'srgb', compositingFormat: 'rgba16f', premultiplied: true, dither: false });
    check(eq(pixel(2, 2), js_test_green), 'render options are applied');
    ui.release(options);

    const layered = ui.frame({ size: ui.sizeFix(4, 4), fill: { layers: [ui.layerSolid(ui.red), ui.layerSolid(ui.blue)] } });
    pixel = await js_test_render(layered, 4, 4);
    check(eq(pixel(0, 0), js_test_blue), 'later layers draw on top');
    ui.release(layered);

    const device = js_test_state.device;
    const profiler = gpu.profilerCreate(device, 8);
    const target = gpu.textureCreate(device, { label: 'profiled', format: 'bgra8', size: [4, 4], usage: gpu.textureUsageRenderTarget | gpu.textureUsageSample });
    const profiled = ui.frame({ size: ui.sizeFix(4, 4), fill: ui.fillSolid(ui.red) });
    ui.setToPxFromPt(1);
    ui.layout(profiled);
    const commandBuffer = gpu.commandBufferCreate(device, 'profiled');
    const profiling = gpu.commandBufferProfile(commandBuffer, profiler);
    ui.render(profiled, commandBuffer, target, {});
    await gpuWait(gpu.commandBufferCommit(commandBuffer));
    const profile = gpu.profilerPoll(profiler);
    check(profiling === false || (profile !== null && profile.passCount > 0 && profile.passNames.length === gpu.profileMaxPasses && profile.passNames[0].length > 0), 'profiles come back as structs');
    ui.release(profiled);
    gpu.textureDestroy(device, target);
    gpu.profilerDestroy(device, profiler);

    const text = ui.frame({ size: ui.sizeFix(32, 32), fill: ui.fillSolid(ui.black), layout: ui.layoutStackX('center', 'center', 0, 0) });
    ui.text({ parent: text, str: ui.str('W', ui.white, 24) });
    pixel = await js_test_render(text, 32, 32);
    let lit = 0;
    for (let y = 0; y < 32; y++) {
        for (let x = 0; x < 32; x++) {
            lit += pixel(x, y)[1] > 32 ? 1 : 0;
        }
    }
    check(lit > 20 && lit < 600, 'text draws glyphs');
    ui.release(text);
});

js_test('render graphics', async () => {
    const device = js_test_state.device;
    const bytes = new Uint8Array([0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255]);
    const graphic = ui.graphicCreate(2, 2, 'bgra8', bytes, 8, true);
    const root = ui.frame({ size: ui.sizeFix(2, 2), fill: { layers: [ui.layerGraphicEx(graphic, 'fill', 'nearest', 1, ui.white, 'srgb', true)] } });
    let pixel = await js_test_render(root, 2, 2);
    check(eq(pixel(0, 0), js_test_red) && eq(pixel(1, 0), js_test_green) && eq(pixel(0, 1), js_test_blue) && eq(pixel(1, 1), js_test_white), 'graphics from bytes sample their pixels');
    ui.release(root);
    ui.graphicDestroy(graphic);

    const texture = gpu.textureCreate(device, { label: 'graphic', format: 'bgra8', size: [2, 2], usage: gpu.textureUsageSample | gpu.textureUsageUpload });
    gpu.textureWrite(device, texture, 0, 0, 2, 2, bytes, 8);
    const wrapped = ui.graphicCreateTexture(texture);
    const scaled = ui.frame({ size: ui.sizeFix(4, 4), fill: { layers: [ui.layerGraphicEx(wrapped, 'fill', 'nearest', 1, ui.white, 'srgb', true)] } });
    pixel = await js_test_render(scaled, 4, 4);
    check(eq(pixel(0, 0), js_test_red) && eq(pixel(3, 0), js_test_green) && eq(pixel(0, 3), js_test_blue) && eq(pixel(3, 3), js_test_white), 'graphics from textures scale to the node');
    ui.release(scaled);
    ui.graphicDestroy(wrapped);

    const big = new Uint8Array(1024 * 1024 * 4);
    for (let i = 0; i < big.length; i += 4) {
        big[i + 1] = 255;
        big[i + 3] = 255;
    }
    const large = ui.graphicCreate(1024, 1024, 'bgra8', big, 4096, true);
    const fromLarge = ui.frame({ size: ui.sizeFix(2, 2), fill: ui.fillGraphic(large, 'fill', ui.white) });
    pixel = await js_test_render(fromLarge, 2, 2);
    check(eq(pixel(1, 1), js_test_green), 'large images upload in one call');
    ui.release(fromLarge);
    ui.graphicDestroy(large);

    const pointer = uniform.alloc(16);
    uniform.write(pointer, bytes);
    const fromPointer = ui.graphicCreate(2, 2, 'bgra8', pointer, 8, true);
    const viaPointer = ui.frame({ size: ui.sizeFix(2, 2), fill: { layers: [ui.layerGraphicEx(fromPointer, 'fill', 'nearest', 1, ui.white, 'srgb', true)] } });
    pixel = await js_test_render(viaPointer, 2, 2);
    check(eq(pixel(1, 0), js_test_green), 'byte parameters accept raw pointers');
    ui.release(viaPointer);
    ui.graphicDestroy(fromPointer);
    uniform.free(pointer);
});

js_test('render custom', async () => {
    const renders = [];
    const root = ui.frame({ size: ui.sizeFix(8, 4), fill: ui.fillSolid(ui.black) });
    ui.custom({
        parent: root,
        size: ui.sizeFix(4, 4),
        desc: {
            onRender(info) {
                renders.push(info);
                const x = info.originPx[0];
                ui.customDrawSolid(info, undefined, ui.green, [
                    { posPx: [x, 0] }, { posPx: [x + 4, 0] }, { posPx: [x, 4] },
                    { posPx: [x + 4, 0] }, { posPx: [x + 4, 4] }, { posPx: [x, 4] },
                ]);
            },
        },
    });
    ui.custom({
        parent: root,
        pos: ui.posXy(4, 0),
        size: ui.sizeFix(4, 4),
        desc: {
            onRender(info) {
                renders.push(info);
                ui.customDrawSolid(info, [1, 0, 0, 1, info.originPx[0], info.originPx[1]], ui.red, new Float32Array([0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 0, 4, 0, 0, 0, 4, 4, 0, 0, 0, 4, 0, 0]));
            },
        },
    });
    const pixel = await js_test_render(root, 8, 4);
    check(renders.length === 2 && eq(renders[0].originPx, [0, 0]) && eq(renders[0].sizePx, [4, 4]) && eq(renders[1].originPx, [4, 0]) && renders[0].drawSolid !== 0, 'onRender receives the pixel rect');
    check(eq(pixel(1, 1), js_test_green) && eq(pixel(2, 3), js_test_green), 'custom vertices from objects draw');
    check(eq(pixel(5, 1), js_test_red) && eq(pixel(7, 3), js_test_red), 'custom vertices from a Float32Array draw through a transform');
    ui.release(root);
});
