'use strict';

function js_test_list_items(view) {
    const items = [];
    const iter = ui.listIter(view);
    let desc;
    while ((desc = ui.listIterNext(iter)) !== null) {
        items.push(desc);
    }
    return items;
}

js_test('list', async () => {
    ui.setToPxFromPt(1);
    const view = ui.listCreate(8);
    ui.listEdit(view, { adds: [0, 1, 2] });
    const measured = [];
    const linear = ui.listLinearLayout({ axis: 'y', size: [200, 300], padding: ui.pad(10), spacing: [5, 5] });
    check(linear.place !== 0 && linear.itemSize !== 0 && linear.scrollAxis === 'y' && eq(linear.size, [200, 300]), 'linear layouts carry native callbacks');

    let prev = ui.nodeNull;
    function frame(layout, onItemUpdate) {
        dispatch(prev, 1.0, []);
        const root = ui.frame({ size: ui.sizeFix(200, 300) });
        ui.listUpdate(view, { node: root, layout: layout, deltaS: 1 / 60, onItemUpdate: onItemUpdate });
        const items = js_test_list_items(view);
        for (const item of items) {
            ui.frame({ parent: root, key: item.index, pos: ui.posXy(item.pos[0], item.pos[1]), size: ui.sizeFix(item.size[0], item.size[1]) });
        }
        ui.layout(root);
        ui.release(prev);
        prev = root;
        return items;
    }
    let items = frame(linear, (index, content, available, result) => {
        measured.push([index, content, available]);
        return { size: [available[0], 40 + index * 10] };
    });
    check(measured.length === 3 && measured[0][1] !== 0 && measured[1][1] !== measured[0][1] && eq(measured[0][2], [180, 280]), 'onItemUpdate measures every item with its content pointer');
    check(items.length === 3 && eq(items[0].pos, [10, 10]) && eq(items[0].size, [180, 40]) && eq(items[1].pos, [10, 55]) && eq(items[2].pos, [10, 110]) && items[2].opacity === 1 && items[2].isRemoved === false, 'the linear layout stacks items with padding and spacing');
    const at = ui.listGetItemAtPosition(view, [20, 60]);
    check(at !== null && at.index === 1 && ui.listGetItemAtPosition(view, [20, 299]) === null, 'items are found at positions');
    const near = ui.listGetItemNearPosition(view, [20, 299]);
    check(near !== null && near.index === 2, 'the nearest item is found past the end');
    check(ui.listGetScrollOffset(view) === 0, 'scroll offset starts at zero');

    uniform.write(measured[1][1], new Uint8Array([9, 8, 7, 6, 5, 4, 3, 2]));
    ui.listEdit(view, { deletes: [0], moves: [{ srcIndex: 1, dstIndex: 0 }], adds: [2] });
    const contents = [];
    items = frame(linear, (index, content, available, result) => {
        contents.push(uniform.read(content, 2));
        return { size: [available[0], 20] };
    });
    check(items.length === 3 && contents[0][0] === 9 && contents[0][1] === 8 && contents[2][0] === 0, 'edits delete, move and add items and keep their content');

    const custom = {
        scrollAxis: 'y',
        size: [200, 300],
        itemAvailable: [200, 300],
        place(layoutItems, count, rebase) {
            for (let i = 0; i < layoutItems.length; i++) {
                layoutItems[i].offset = [i * 3, i * 50];
            }
            return [200, count * 50];
        },
        itemSize(size, available) {
            return [size[0] / 2, size[1]];
        },
    };
    items = frame(custom, (index, content, available, result) => ({ size: [100, 20] }));
    check(items.length === 3 && eq(items[0].pos, [0, 0]) && eq(items[0].size, [50, 20]) && eq(items[1].pos, [3, 50]) && eq(items[2].pos, [6, 100]), 'JavaScript layouts place items through callbacks');

    ui.release(prev);
    dispatch(ui.nodeNull, 2.0, []);
    ui.listDestroy(view);
});

js_test('text input', async () => {
    ui.setToPxFromPt(1);
    const input = ui.textInputCreate();
    ui.textInputSetText(input, 'héllo');
    check(ui.textInputGetText(input) === 'héllo' && ui.textInputIsFocused(input) === false, 'text round trips through utf8');
    const layout = { width: 300, ptSize: 13, weight: 400, padding: [8, 8, 6, 6], multiline: true };
    let prev = ui.nodeNull;
    let timestamp = 1.0;
    function frame(events) {
        timestamp += 0.1;
        dispatch(prev, timestamp, events);
        const root = ui.frame({ size: ui.sizeFix(600, 500) });
        const result = ui.textInputOnUpdate(input, root, root, layout, 1 / 60);
        ui.layout(root);
        ui.release(prev);
        prev = root;
        return result;
    }
    ui.textInputSetFocus(input, true);
    let result = frame([]);
    check(result.isFocused && ui.textInputIsFocused(input) && result.text === 'héllo' && result.caret.height > 0 && result.caretVisible && result.main !== 0 && result.content !== 0 && result.height > 0, 'onUpdate reports focus, text and the caret');
    frame([{ type: 'text', text: { utf8: '!' } }]);
    result = frame([]);
    check(ui.textInputGetText(input) === 'héllo!' && result.text === 'héllo!', 'typed text inserts at the caret');
    frame([{ type: 'textCommand', textCommand: { command: 'selectAll' } }]);
    result = frame([]);
    check(result.selectionRects.length === 1 && result.selectionRects[0].width > 0 && result.markedRects.length === 0, 'selections report their rects');
    frame([{ type: 'textCommand', textCommand: { command: 'deleteBackward' } }]);
    result = frame([]);
    check(result.text === '' && ui.textInputGetText(input) === '', 'commands edit the text');
    ui.textInputSetFocus(input, false);
    result = frame([]);
    check(result.isFocused === false && ui.inputGetFocus() === 0, 'focus clears');
    ui.release(prev);
    dispatch(ui.nodeNull, timestamp + 1, []);
    ui.textInputDestroy(input);
});

js_test('physics', async () => {
    ui.setToPxFromPt(1);
    const scroll = uniform.alloc(uniform.sizeof('ui_scroll'));
    ui.scrollInit(scroll);
    check(uniform.readStruct('ui_scroll', scroll).phase === 'idle' && ui.scrollGetOffset(scroll) === 0, 'scroll state starts idle');
    ui.scrollOnWheel(scroll, 30);
    for (let i = 0; i < 10; i++) {
        ui.scrollOnUpdate(scroll, 0, 100, 0, 1000, 1 / 60);
    }
    const state = uniform.readStruct('ui_scroll', scroll);
    check(ui.scrollGetOffset(scroll) > 0 && ui.scrollGetOffset(scroll) <= 30 && (state.phase === 'wheel' || state.phase === 'idle'), 'wheel deltas scroll the content');
    check(ui.scrollGetOffset(state) === ui.scrollGetOffset(scroll), 'const state parameters accept objects');
    uniform.free(scroll);

    const slide = uniform.alloc(uniform.sizeof('ui_slide'));
    ui.slideInit(slide, 'trigger', 'actions');
    let slides = 0;
    ui.slideSetOnSlide(slide, () => slides++);
    ui.slideSetOnSlide(slide, () => slides += 10);
    const s = uniform.readStruct('ui_slide', slide);
    check(s.modeLeft === 'trigger' && s.modeRight === 'actions' && s.phase === 'idle' && s.onSlide !== 0 && ui.slideGetOffset(slide) === 0, 'slide state initializes with its callback');
    ui.slideOnUpdate(slide, 200, 50, 1 / 60);
    check(ui.slideGetOffset(slide) === 0 && slides === 0, 'an idle slide stays put');
    uniform.free(slide);
});

js_test('hud', async () => {
    ui.setToPxFromPt(1);
    const h = hud.create(js_test_state.device, js_test_state.font);
    hud.set(h);
    hud.setEnabled(true);
    let prev = ui.nodeNull;
    function frame() {
        dispatch(prev, 0, []);
        const root = ui.frame({ size: ui.sizeFix(800, 600) });
        hud.onUpdate(root, true, 1 / 60);
        ui.layout(root);
        ui.release(prev);
        prev = root;
    }
    const v1 = hud.slider('js slider', 0, 100, 25, 'float');
    frame();
    const v2 = hud.slider('js slider', 0, 200, 25, 'float');
    frame();
    check(v1 === 25 && v2 === 50, 'sliders keep their relative value when the range changes');
    const s1 = hud.select('js select', 5, 4);
    frame();
    const s2 = hud.select('js select', 3, 0);
    frame();
    check(s1 === 4 && s2 === 0, 'selects clamp stale options');
    const label = hud.selectLabels('js labels', 1, ['one', 'two', 'three']);
    const toggle = hud.toggle('js toggle', true);
    const text = hud.textInput('js text', 'initial');
    hud.label('js label', 'value');
    const button = hud.button('js button', 'press');
    const stepper = hud.stepper('js stepper', 4, 2);
    frame();
    check(label === 1 && toggle === true && text === 'initial' && button === false && stepper === 2, 'controls return their values');
    ui.release(prev);
    dispatch(ui.nodeNull, 1, []);
    hud.destroy(h);
    check(hud.get() === 0, 'destroying the current hud clears it');
});

js_test('log', async () => {
    check(log.init() === 0 && log.isActive(), 'the log initializes in memory');
    log.str('first line\nsecond line');
    log.s('third €');
    check(log.viewRowCount(80) === 3, 'rows count wrapped lines');
    const glyphs = [];
    log.viewGlyphs(80, -3, 0, (row, col, cp) => glyphs.push([row, col, cp]));
    const rows = new Map();
    for (const [row, col, cp] of glyphs) {
        rows.set(row, (rows.get(row) || '') + String.fromCodePoint(cp));
    }
    check(rows.get(-3) === 'first line\n' && rows.get(-2) === 'second line' && rows.get(-1) === 'third €', 'glyph callbacks deliver every character');
    log.setPref('width', 42);
    check(log.getPref('width') === 42, 'prefs round trip');
    log.clear();
    check(log.viewRowCount(80) === 0, 'clear empties the log');
    log.deinit();
    check(log.isActive() === false, 'deinit stops the log');
});
