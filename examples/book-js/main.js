'use strict';

const sidebarWidth = 220;
const sidebarPadding = 8;
const sidebarTopInset = 38;
const rowHeight = 30;
const rowSpacing = 2;
const rowFontSize = 13;
const bodyFontSize = 17;
const columnMaxWidth = 660;
const columnPadding = 40;
const columnPaddingTop = 40;
const columnPaddingBottom = 80;
const scrollbarWidth = 6;
const scrollbarMargin = 3;
const scrollbarMinLength = 32;
const measureChunk = 256;

let prevRoot = ui.nodeNull;
let mainWindow = 0;
let fontInter = 0;
let fontMono = 0;
let source = null;
let files = [];
let selected = 0;
let sidebarScroll = 0;

function themeFor(isDark) {
    if (isDark) {
        return {
            background: ui.gray(0.11),
            sidebar: ui.gray(0.145),
            separator: ui.grayA(1, 0.08),
            text: ui.gray(0.88),
            secondary: ui.gray(0.55),
            accent: ui.hex(0x3b82f6),
            rowHover: ui.grayA(1, 0.07),
            rowPressed: ui.grayA(1, 0.12),
            thumb: ui.grayA(1, 0.28),
        };
    }
    return {
        background: ui.white,
        sidebar: ui.gray(0.96),
        separator: ui.grayA(0, 0.08),
        text: ui.gray(0.13),
        secondary: ui.gray(0.5),
        accent: ui.hex(0x2f6fed),
        rowHover: ui.grayA(0, 0.06),
        rowPressed: ui.grayA(0, 0.1),
        thumb: ui.grayA(0, 0.25),
    };
}

function windowTitle() {
    const file = files[selected];
    return file === undefined ? 'Text Viewer' : file.name + ' — Text Viewer';
}

function select(index) {
    if (index < 0 || index >= files.length || index === selected) {
        return;
    }
    selected = index;
    if (mainWindow !== 0) {
        app.windowSetTitle(mainWindow, windowTitle());
    }
}

async function loadFile(name) {
    try {
        const response = await fetch(name);
        if (response.ok === false) {
            return null;
        }
        const text = await response.text();
        return { name: name, blocks: text.replace(/\r\n?/g, '\n').replace(/\t/g, '    ').split('\n'), scroll: 0, measure: null };
    } catch (error) {
        return null;
    }
}

async function loadFiles() {
    const loaded = [];
    for (let i = 1; ; i++) {
        const file = await loadFile(i + '.txt');
        if (file === null) {
            return loaded;
        }
        loaded.push(file);
    }
}

function scrollCreate() {
    const state = uniform.alloc(uniform.sizeof('ui_scroll'));
    ui.scrollInit(state);
    return state;
}

function scrollAttach(node, state) {
    ui.onScroll({ node: node, func: (event) => ui.scrollOnWheel(state, event.delta[1]) });
    ui.pan({
        node: node,
        directOnly: true,
        onChange: (gesture) => {
            if (ui.scrollOnPan(state, gesture, 'y')) {
                ui.panGestureClaim(gesture);
            }
        },
    });
}

function scrollUpdate(state, viewLength, contentLength, deltaS) {
    const offset = uniform.readStruct('ui_scroll', state).offset;
    ui.scrollOnUpdate(state, offset, offset + viewLength, 0, contentLength, deltaS);
    return ui.scrollGetOffset(state);
}

function scrollbar(parent, size, contentLength, offset, color) {
    const viewLength = size[1];
    if (contentLength <= viewLength + 0.5) {
        return;
    }
    const trackLength = viewLength - 2 * scrollbarMargin;
    const thumbLength = Math.max(scrollbarMinLength, Math.min(trackLength, trackLength * viewLength / contentLength));
    const t = Math.min(1, Math.max(0, offset / (contentLength - viewLength)));
    ui.rect({
        parent: parent,
        pos: ui.posXy(size[0] - scrollbarWidth - scrollbarMargin, ui.roundPx(scrollbarMargin + t * (trackLength - thumbLength))),
        size: ui.sizeFix(scrollbarWidth, thumbLength),
        corners: ui.corners(scrollbarWidth / 2),
        fill: ui.fillSolid(color),
    });
}

function bodyStr(text, color, font) {
    const str = ui.str(text, color, bodyFontSize);
    str.attributes.push({ start: 0, end: 4294967295, type: 'font', font: font });
    return str;
}

function measure(blocks, width, scale, font) {
    const probeRoot = ui.frame({ layout: ui.layoutStackY('start', 'start', 0, 0) });
    const probe = ui.text({ parent: probeRoot, str: bodyStr('Xg', ui.black, font) });
    ui.layout(probeRoot);
    const lineHeight = ui.getSize(probe)[1];
    ui.release(probeRoot);

    const offsets = new Float64Array(blocks.length + 1);
    for (let start = 0; start < blocks.length; start += measureChunk) {
        const end = Math.min(blocks.length, start + measureChunk);
        const root = ui.frame({ layout: ui.layoutStackY('start', 'start', 0, 0) });
        const nodes = [];
        for (let i = start; i < end; i++) {
            nodes.push(blocks[i].length === 0 ? ui.nodeNull : ui.text({ parent: root, size: { width: ui.sizeAxisFix(width) }, str: bodyStr(blocks[i], ui.black, font) }));
        }
        ui.layout(root);
        for (let i = start; i < end; i++) {
            const node = nodes[i - start];
            offsets[i + 1] = offsets[i] + (node === ui.nodeNull ? lineHeight : ui.getSize(node)[1]);
        }
        ui.release(root);
    }
    return { width: width, scale: scale, font: font, lineHeight: lineHeight, offsets: offsets };
}

function firstBlock(offsets, y) {
    let lo = 0;
    let hi = offsets.length - 1;
    while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (offsets[mid + 1] <= y) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

function remeasure(file, width, scale, font) {
    const previous = file.measure;
    const next = measure(file.blocks, width, scale, font);
    file.measure = next;
    if (previous === null) {
        return;
    }
    const state = uniform.readStruct('ui_scroll', file.scroll);
    const top = state.offset - columnPaddingTop;
    const index = firstBlock(previous.offsets, top);
    if (index === file.blocks.length) {
        return;
    }
    const fraction = (top - previous.offsets[index]) / Math.max(previous.offsets[index + 1] - previous.offsets[index], 1);
    state.offset = columnPaddingTop + next.offsets[index] + fraction * (next.offsets[index + 1] - next.offsets[index]);
    uniform.writeStruct('ui_scroll', file.scroll, state);
}

function textView(parent, file, size, deltaS, theme, font) {
    const viewport = ui.frame({
        parent: parent,
        key: 'viewport',
        size: ui.sizeFix(size[0], size[1]),
        clip: true,
    });
    const columnWidth = Math.max(80, Math.min(columnMaxWidth, size[0] - 2 * columnPadding));
    const columnX = ui.roundPx((size[0] - columnWidth) / 2);
    const scale = ui.getToPxFromPt();
    const current = file.measure;
    if (current === null || current.width !== columnWidth || current.scale !== scale || current.font !== font) {
        remeasure(file, columnWidth, scale, font);
    }
    const offsets = file.measure.offsets;
    const count = file.blocks.length;
    const contentLength = columnPaddingTop + offsets[count] + columnPaddingBottom;
    scrollAttach(viewport, file.scroll);
    const offset = scrollUpdate(file.scroll, size[1], contentLength, deltaS);
    const top = offset - columnPaddingTop;
    for (let i = firstBlock(offsets, top); i < count && offsets[i] < top + size[1]; i++) {
        if (file.blocks[i].length > 0) {
            ui.text({
                parent: viewport,
                pos: ui.posXy(columnX, ui.roundPx(columnPaddingTop + offsets[i] - offset)),
                size: { width: ui.sizeAxisFix(columnWidth) },
                str: bodyStr(file.blocks[i], theme.text, font),
            });
        }
    }
    scrollbar(viewport, size, contentLength, offset, theme.thumb);
}

function message(parent, size, text, theme) {
    const box = ui.frame({
        parent: parent,
        size: ui.sizeFix(size[0], size[1]),
        layout: ui.layoutStackY('center', 'center', 0, ui.pad(32)),
    });
    ui.text({
        parent: box,
        size: { width: ui.sizeAxisFix(Math.max(80, Math.min(440, size[0] - 64))) },
        str: ui.str(text, theme.secondary, 14),
    });
}

function sidebarRow(panel, index, y, theme) {
    const node = ui.node(panel, index);
    const tap = ui.tap({
        node: node,
        onChange: (gesture) => {
            if (ui.tapGestureGetPhase(gesture) === 'recognized') {
                select(index);
            }
        },
    });
    const hover = ui.hover({ node: node });
    const isSelected = index === selected;
    let fill;
    if (isSelected) {
        fill = theme.accent;
    } else if (ui.tapGestureHasTouch(tap)) {
        fill = theme.rowPressed;
    } else if (ui.hoverGestureHasHover(hover)) {
        fill = theme.rowHover;
    }
    ui.frame({
        node: node,
        pos: ui.posXy(sidebarPadding, y),
        size: ui.sizeFix(sidebarWidth - 2 * sidebarPadding, rowHeight),
        layout: ui.layoutStackX('start', 'center', 0, ui.padXy(10, 0)),
        corners: ui.corners(6),
        fill: fill === undefined ? undefined : ui.fillSolid(fill),
        clip: true,
    });
    ui.text({
        parent: node,
        str: ui.strWeight(files[index].name, isSelected ? ui.white : theme.text, rowFontSize, isSelected ? 600 : 450),
    });
}

function sidebar(parent, height, deltaS, theme) {
    const panel = ui.frame({
        parent: parent,
        key: 'sidebar',
        size: ui.sizeFix(sidebarWidth, height),
        fill: ui.fillSolid(theme.sidebar),
        clip: true,
    });
    const contentLength = sidebarTopInset + 2 * sidebarPadding + files.length * rowHeight + Math.max(0, files.length - 1) * rowSpacing;
    scrollAttach(panel, sidebarScroll);
    const offset = scrollUpdate(sidebarScroll, height, contentLength, deltaS);
    for (let i = 0; i < files.length; i++) {
        const y = sidebarTopInset + sidebarPadding + i * (rowHeight + rowSpacing) - offset;
        if (y + rowHeight >= 0 && y <= height) {
            sidebarRow(panel, i, ui.roundPx(y), theme);
        }
    }
    scrollbar(panel, [sidebarWidth, height], contentLength, offset, theme.thumb);
    ui.rect({
        parent: panel,
        pos: ui.posXy(sidebarWidth - 1, 0),
        size: ui.sizeFix(1, height),
        fill: ui.fillSolid(theme.separator),
    });
}

function handleKeys(events, viewLength) {
    for (const event of events) {
        if (event.type !== 'key' || event.key.down === false) {
            continue;
        }
        const file = files[selected];
        const line = file === undefined || file.measure === null ? 0 : file.measure.lineHeight;
        const page = Math.max(line, viewLength - 2 * line);
        const shift = (event.key.modifiers & app.modifierShift) !== 0;
        let delta = 0;
        switch (event.key.key) {
        case 'down': delta = 3 * line; break;
        case 'up': delta = -3 * line; break;
        case 'pageDown': delta = page; break;
        case 'pageUp': delta = -page; break;
        case 'space': delta = shift ? -page : page; break;
        case 'home': delta = -1e9; break;
        case 'end': delta = 1e9; break;
        case 'left': select(selected - 1); break;
        case 'right': select(selected + 1); break;
        default: break;
        }
        if (delta !== 0 && file !== undefined) {
            ui.scrollOnWheel(file.scroll, delta);
        }
    }
}

function onUpdate(window) {
    const targetDesc = app.windowGetTargetDesc(window);
    const size = targetDesc.ptSize;
    const timestamp = app.windowGetTimestamp(window);
    const deltaS = app.windowGetDelta(window);
    const isDark = app.windowGetIsDark(window);
    const theme = themeFor(isDark);

    hud.frameBegin(timestamp);

    ui.setToPxFromPt(targetDesc.toPxFromPt);
    const events = app.windowTakeEvents(window);
    handleKeys(events, size[1]);
    ui.inputDispatch(prevRoot, timestamp, events, 1.0);
    app.windowSetCursor(window, ui.inputGetCursor());

    const font = hud.toggle('Mono text', false) ? fontMono : fontInter;

    const root = ui.frame({
        size: ui.sizeFix(size[0], size[1]),
        layout: ui.layoutStackX('start', 'start', 0, 0),
        fill: ui.fillSolid(theme.background),
    });

    sidebar(root, size[1], deltaS, theme);

    const contentSize = [Math.max(0, size[0] - sidebarWidth), size[1]];
    const file = files[selected];
    if (file === undefined) {
        message(root, contentSize, 'No text files found.\n\nPut files named 1.txt, 2.txt, and so on next to main.js.', theme);
    } else {
        textView(root, file, contentSize, deltaS, theme, font);
    }

    hud.frameLabel(file === undefined ? 'no files' : file.name);
    hud.onUpdate(root, isDark, deltaS);

    ui.layout(root);

    ui.release(prevRoot);
    prevRoot = root;

    const commandBuffer = gpu.commandBufferCreate(app.getDevice(app.windowGetApp(window)), '');
    hud.frameProfile(commandBuffer);
    const target = app.windowAcquireTarget(window);
    ui.render(root, commandBuffer, target, ui.utilWindowRenderDesc(window));
    app.windowPresent(window, commandBuffer, target);

    hud.frameEnd();
}

function onStart(a, inter, mono) {
    globalThis.reloadApp = a;
    ui.init(app.getDevice(a));
    ui.inputSet(ui.inputCreate());
    fontInter = ui.fontCreate(inter);
    fontMono = ui.fontCreate(mono);
    ui.setDefaultFont(fontInter);
    hud.set(hud.create(app.getDevice(a), fontMono));
    sidebarScroll = scrollCreate();
    for (const file of files) {
        file.scroll = scrollCreate();
    }

    mainWindow = app.windowCreate(a, {
        onUpdate: onUpdate,
        onCloseRequest: (window) => app.quit(app.windowGetApp(window)),
        title: windowTitle(),
        size: [1040, 700],
        center: true,
        resizable: true,
    });
    app.windowFocus(mainWindow);
}

function onQuit(a) {
    ui.release(prevRoot);
    hud.destroy(hud.get());
    ui.inputDestroy(ui.inputGet());
    for (const file of files) {
        uniform.free(file.scroll);
    }
    uniform.free(sidebarScroll);
    ui.fontDestroy(fontMono);
    ui.fontDestroy(fontInter);
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
    const [inter, mono, loaded] = await Promise.all([
        fetch('Inter.ttf').then((response) => response.arrayBuffer()),
        fetch('JetBrainsMono.ttf').then((response) => response.arrayBuffer()),
        loadFiles(),
    ]);
    files = loaded;

    const desc = { onStart: (a) => onStart(a, inter, mono), onQuit: onQuit };
    if (globalThis.reloadApp === undefined) {
        app.run(desc);
        reload();
    } else {
        app.restart(globalThis.reloadApp, desc);
    }
}

main();
