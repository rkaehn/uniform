'use strict';

let prevRoot = ui.nodeNull;
let source = null;

function onUpdate(window) {
    const targetDesc = app.windowGetTargetDesc(window);

    ui.utilWindowSync(window, prevRoot, 1.0);

    const root = ui.frame({
        size: ui.sizeFix(targetDesc.ptSize[0], targetDesc.ptSize[1]),
        fill: ui.fillSolid(app.windowGetIsDark(window) ? ui.black : ui.white),
    });

    ui.layout(root);

    ui.release(prevRoot);
    prevRoot = root;

    const commandBuffer = gpu.commandBufferCreate(app.getDevice(app.windowGetApp(window)), '');
    const target = app.windowAcquireTarget(window);
    ui.render(root, commandBuffer, target, ui.utilWindowRenderDesc(window));
    app.windowPresent(window, commandBuffer, target);
}

function onStart(a) {
    globalThis.reloadApp = a;
    ui.init(app.getDevice(a));
    ui.inputSet(ui.inputCreate());
    app.windowCreate(a, {
        onUpdate: onUpdate,
        onCloseRequest: (window) => app.quit(app.windowGetApp(window)),
        title: 'uniform',
        size: [640, 480],
        center: true,
        resizable: true,
    });
}

function onQuit(a) {
    ui.release(prevRoot);
    ui.inputDestroy(ui.inputGet());
    ui.deinit();
}

async function reload() {
    const text = await (await fetch('main.js')).text();
    if (source !== null && text !== source) {
        try {
            new Function(text)();
        } catch (error) {
            console.error(error);
        }
    }
    source = text;
    setTimeout(reload, 250);
}

function main() {
    const desc = { onStart: onStart, onQuit: onQuit };
    if (globalThis.reloadApp === undefined) {
        app.run(desc);
        reload();
    } else {
        app.restart(globalThis.reloadApp, desc);
    }
}

main();
