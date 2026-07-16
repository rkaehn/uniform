'use strict';

let prevRoot = ui.nodeNull;
let font = 0;
let source = null;

function onTap(gesture) {
    if (ui.tapGestureGetPhase(gesture) === 'recognized') {
        console.log('tap');
    }
}

function onUpdate(window) {
    const targetDesc = app.windowGetTargetDesc(window);
    const isDark = app.windowGetIsDark(window);

    ui.utilWindowSync(window, prevRoot, 1.0);

    const size = targetDesc.ptSize;
    const bg = isDark ? ui.black : ui.white;
    const fg = isDark ? ui.white : ui.black;

    const root = ui.frame({
        size: ui.sizeFix(size[0], size[1]),
        fill: ui.fillSolid(bg),
    });

    const node = ui.node(root);

    const tap = ui.tap({ node: node, onChange: onTap });
    const hover = ui.hover({ node: node });

    let fill = fg;
    if (ui.tapGestureHasTouch(tap)) {
        fill = ui.red;
    } else if (ui.hoverGestureHasHover(hover)) {
        fill = ui.gray50;
    }

    ui.frame({
        node: node,
        pos: ui.posCenter(),
        layout: ui.layoutStackX('center', 'center', 0, ui.padXy(16, 8)),
        fill: ui.fillSolid(fill),
    });

    ui.text({
        parent: node,
        str: ui.str('Button', bg, 13),
    });

    ui.layout(root);
    ui.release(prevRoot);
    prevRoot = root;

    const commandBuffer = gpu.commandBufferCreate(app.getDevice(app.windowGetApp(window)), 'simple');
    const target = app.windowAcquireTarget(window);
    ui.render(root, commandBuffer, target, ui.utilWindowRenderDesc(window));
    app.windowPresent(window, commandBuffer, target);
}

function onStart(a, fontData) {
    globalThis.reloadApp = a;
    ui.init(app.getDevice(a));
    ui.inputSet(ui.inputCreate());
    font = ui.fontCreate(fontData);
    ui.setDefaultFont(font);

    app.windowCreate(a, {
        onUpdate: onUpdate,
        onCloseRequest: (window) => app.quit(app.windowGetApp(window)),
        title: 'simple',
        size: [480, 320],
        center: true,
        resizable: true,
    });
}

function onQuit(a) {
    ui.release(prevRoot);
    ui.fontDestroy(font);
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

async function main() {
    const fontData = await (await fetch('JetBrainsMono.ttf')).arrayBuffer();

    const desc = { onStart: (a) => onStart(a, fontData), onQuit: onQuit };
    if (globalThis.reloadApp === undefined) {
        app.run(desc);
        reload();
    } else {
        app.restart(globalThis.reloadApp, desc);
    }
}

main();
