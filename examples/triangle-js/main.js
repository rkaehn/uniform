'use strict';

const textureSize = 512;

let prevRoot = ui.nodeNull;
let pipeline = 0;
let texture = 0;
let graphic = 0;

function onUpdate(window) {
    const device = app.getDevice(app.windowGetApp(window));
    const targetDesc = app.windowGetTargetDesc(window);
    const isDark = app.windowGetIsDark(window);

    ui.utilWindowSync(window, prevRoot, 1.0);

    const size = targetDesc.ptSize;

    const root = ui.frame({
        size: ui.sizeFix(size[0], size[1]),
        layout: ui.layoutStackY('center', 'center', 0, 0),
        fill: ui.fillSolid(isDark ? ui.black : ui.white),
    });

    ui.frame({
        parent: root,
        size: ui.sizeFix(256, 256),
        fill: ui.fillGraphic(graphic, 'fill', ui.white),
    });

    ui.layout(root);
    ui.release(prevRoot);
    prevRoot = root;

    const commandBuffer = gpu.commandBufferCreate(device, 'triangle');

    const pass = gpu.commandBufferBeginRender(commandBuffer, texture, 'clear', 'triangle');
    gpu.renderPassSetPipeline(pass, pipeline);
    gpu.renderPassDraw(pass, 0, 3);
    gpu.renderPassEnd(pass);

    const target = app.windowAcquireTarget(window);
    ui.render(root, commandBuffer, target, ui.utilWindowRenderDesc(window));
    app.windowPresent(window, commandBuffer, target);
}

function onStart(a, shaderData) {
    const device = app.getDevice(a);

    ui.init(device);
    ui.inputSet(ui.inputCreate());

    const library = gpu.libraryCreate(device, shaderData, 'triangle');
    pipeline = gpu.renderPipelineCreate(device, {
        library: library,
        vertexName: 'triangle_v',
        fragmentName: 'triangle_f',
        colorFormat: 'bgra8',
    });
    gpu.libraryDestroy(device, library);

    texture = gpu.textureCreate(device, {
        label: 'triangle',
        format: 'bgra8',
        size: [textureSize, textureSize],
        usage: gpu.textureUsageRenderTarget | gpu.textureUsageSample,
    });

    graphic = ui.graphicCreateTexture(texture);

    app.windowCreate(a, {
        onUpdate: onUpdate,
        onCloseRequest: (window) => app.quit(app.windowGetApp(window)),
        title: 'triangle',
        size: [640, 480],
        center: true,
        resizable: true,
    });
}

function onQuit(a) {
    const device = app.getDevice(a);

    ui.release(prevRoot);
    ui.graphicDestroy(graphic);
    gpu.renderPipelineDestroy(device, pipeline);
    ui.inputDestroy(ui.inputGet());
    ui.deinit();
}

async function main() {
    const shaderData = new Uint8Array(await (await fetch('triangle_shaders' + gpu.libraryGetExtension())).arrayBuffer());

    app.run({ onStart: (a) => onStart(a, shaderData), onQuit: onQuit });
}

main();
