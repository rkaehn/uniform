'use strict';

function js_test_zone(root) {
    return ui.frame({ parent: root, key: 'zone', pos: ui.posXy(0, 0), size: ui.sizeFix(400, 400) });
}

function js_test_root() {
    const root = ui.frame({ size: ui.sizeFix(400, 400) });
    return { root: root, zone: js_test_zone(root) };
}

js_test('gestures', async () => {
    const phases = [];
    let prev = ui.nodeNull;
    let hadTouch = false;
    function frame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        const tap = ui.tap({ node: t.zone, onChange: (g) => phases.push(ui.tapGestureGetPhase(g)) });
        hadTouch = hadTouch || ui.tapGestureHasTouch(tap);
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
        return tap;
    }
    frame(1.0, []);
    frame(1.1, [touchEvent('touchBegan', 1, [100, 100], 1.1)]);
    check(hadTouch && ui.tapGestureGetTouchCount(frame(1.15, [])) === 1, 'a tap reports its touch while it is down');
    frame(1.2, [touchEvent('touchEnded', 1, [100, 100], 1.2)]);
    const tap = frame(1.3, []);
    check(phases.indexOf('recognized') >= 0 && ui.tapGestureGetPhase(tap) === 'possible', 'a tap recognizes through its callback and settles');
    check(eq(ui.tapGestureGetPosition(tap), [100, 100]), 'a tap remembers its position');

    const pans = [];
    let translation = [0, 0];
    let velocity = [0, 0];
    function panFrame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        const pan = ui.pan({ node: t.zone, onChange: (g) => { pans.push(ui.panGestureGetPhase(g)); translation = ui.panGestureGetTranslation(g, false); velocity = ui.panGestureGetVelocity(g); } });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
        return pan;
    }
    panFrame(2.0, []);
    panFrame(2.1, [touchEvent('touchBegan', 2, [100, 100], 2.1)]);
    panFrame(2.2, [touchEvent('touchMoved', 2, [140, 100], 2.2)]);
    panFrame(2.3, [touchEvent('touchMoved', 2, [180, 110], 2.3)]);
    check(pans.indexOf('began') >= 0 && pans.indexOf('changed') >= 0 && translation[0] > 30 && translation[0] <= 80 && translation[1] > 0 && translation[1] <= 10 && velocity[0] > 0, 'a pan reports translation and velocity');
    panFrame(2.4, [touchEvent('touchEnded', 2, [180, 110], 2.4)]);
    check(pans[pans.length - 1] === 'ended', 'a pan ends when the touch lifts');

    let hover = 0;
    let hovered = false;
    function hoverFrame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        hover = ui.hover({ node: t.zone });
        hovered = ui.hoverGestureHasHover(hover);
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    hoverFrame(3.0, []);
    hoverFrame(3.1, [hoverEvent('hoverBegan', 3, [50, 50], 3.1)]);
    hoverFrame(3.2, [hoverEvent('hoverMoved', 3, [52, 50], 3.2)]);
    check(hovered && ui.hoverGestureGetPhase(hover) === 'changed' && eq(ui.hoverGestureGetPosition(hover), [52, 50]), 'a hover tracks the pointer');
    hoverFrame(3.3, [hoverEvent('hoverEnded', 3, [52, 50], 3.3)]);
    hoverFrame(3.4, []);
    check(hovered === false, 'a hover ends with the pointer');

    const presses = [];
    function pressFrame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.longPress({ node: t.zone, duration: 0.3, onChange: (g) => presses.push(ui.longPressGestureGetPhase(g)) });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    pressFrame(4.0, []);
    pressFrame(4.1, [touchEvent('touchBegan', 4, [100, 100], 4.1)]);
    pressFrame(4.2, []);
    pressFrame(4.6, []);
    check(presses.indexOf('began') >= 0, 'a long press begins after its duration');
    pressFrame(4.7, [touchEvent('touchEnded', 4, [100, 100], 4.7)]);
    pressFrame(4.8, []);
    check(presses[presses.length - 1] === 'ended', 'a long press ends on release');

    ui.release(prev);
    dispatch(ui.nodeNull, 5.0, []);
});

js_test('custom gesture', async () => {
    const state = new Map();
    const calls = { created: 0, destroyed: 0, resets: 0, pointer: 0, canceled: 0 };
    let position = [0, 0];
    let moving = null;
    const functions = {
        create(gesture) { calls.created++; state.set(gesture, { touches: 0 }); },
        destroy(gesture) { calls.destroyed++; state.delete(gesture); },
        reset(gesture) { calls.resets++; },
        accepts(gesture, event, pointer) { return event.type === 'touchBegan' ? 'accept' : 'reject'; },
        onPointer(gesture, event, pointer) {
            calls.pointer++;
            position = ui.inputPointerGetPosition(pointer);
            moving = ui.inputPointerIsMoving(pointer);
            state.get(gesture).touches++;
            if (event.type === 'touchEnded') {
                ui.gestureSetPhase(gesture, 'recognized');
            }
        },
        cancel(gesture) { calls.canceled++; },
    };
    let prev = ui.nodeNull;
    let gesture = 0;
    let zone = ui.nodeNull;
    function frame(timestamp, events, declare = true) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        zone = t.zone;
        if (declare) {
            gesture = ui.gesture({ node: t.zone, name: 'custom', ...functions });
        }
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    frame(1.0, []);
    check(calls.created === 1 && ui.gestureGetPhase(gesture) === 'possible' && ui.gestureGetInput(gesture) === ui.inputGet() && ui.gestureGetName(gesture) === 'custom' && state.size === 1, 'a gesture runs its own functions');
    check(ui.inputGetGesture(ui.gestureGetKey(gesture)) === gesture && ui.nodeIsEqual(ui.gestureGetNode(gesture), zone), 'inputGetGesture finds a gesture by key');
    frame(1.1, [touchEvent('touchBegan', 11, [200, 200], 1.1)]);
    check(calls.pointer === 1 && eq(position, [200, 200]) && moving === false && ui.gestureGetPhase(gesture) === 'possible', 'an accepted touch reaches onPointer with pointer queries');
    frame(1.2, [touchEvent('touchMoved', 11, [210, 200], 1.2)]);
    frame(1.3, [touchEvent('touchEnded', 11, [210, 200], 1.3)]);
    check(calls.pointer === 3 && calls.resets === 1 && ui.gestureGetPhase(gesture) === 'possible' && state.get(gesture).touches === 3, 'a recognized phase settles with a reset');
    frame(1.4, [touchEvent('touchBegan', 12, [200, 200], 1.4)]);
    frame(1.5, [], false);
    check(calls.destroyed === 0 && state.size === 1, 'a gesture survives until the next dispatch sweeps it');
    frame(1.6, [], false);
    check(calls.destroyed === 1 && calls.canceled === 1 && state.size === 0, 'a gesture left out of a rebuild is canceled and destroyed through the sweep');

    const seen = [];
    let marks = 0;
    function markFrame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.tap({ node: t.zone, onChange: (g) => seen.push(ui.tapGestureGetPhase(g)) });
        ui.gesture({
            node: t.zone,
            name: 'mark',
            simultaneous: (g, other) => ui.gestureGetName(other) === 'tap',
            accepts: (g, event) => event.type === 'touchBegan' ? 'accept' : 'reject',
            onPointer: (g, event) => {
                if (event.type === 'touchEnded') {
                    marks++;
                    ui.gestureSetPhase(g, 'recognized');
                }
            },
        });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    markFrame(2.0, []);
    markFrame(2.1, [touchEvent('touchBegan', 21, [120, 240], 2.1)]);
    markFrame(2.2, [touchEvent('touchEnded', 21, [120, 240], 2.2)]);
    markFrame(2.3, []);
    check(seen.indexOf('recognized') >= 0 && marks === 1, 'a gesture that names the tap simultaneous recognizes alongside it');

    let claimed = 0;
    function claimFrame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.gesture({ node: t.zone, name: 'claimer', accepts: () => 'accept', onPointer: (g, event) => { if (event.type === 'touchBegan') { ui.gestureClaim(g); claimed++; } } });
        ui.tap({ node: t.zone, onChange: (g) => seen.push('tap ' + ui.tapGestureGetPhase(g)) });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    claimFrame(3.0, []);
    claimFrame(3.1, [touchEvent('touchBegan', 31, [120, 240], 3.1)]);
    claimFrame(3.2, [touchEvent('touchEnded', 31, [120, 240], 3.2)]);
    claimFrame(3.3, []);
    check(claimed === 1 && seen.indexOf('tap recognized') < 0, 'a claimed touch is taken from other gestures');

    ui.release(prev);
    dispatch(ui.nodeNull, 4.0, []);
});

js_test('input', async () => {
    const pointerEvents = [];
    const scrolls = [];
    const texts = [];
    let prev = ui.nodeNull;
    function frame(timestamp, events) {
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.onPointer({ node: t.zone, func: (event, pointer) => pointerEvents.push(event.type + ' ' + ui.pointerIsNull(pointer)) });
        ui.onScroll({ node: t.zone, func: (event) => scrolls.push(event.delta) });
        ui.onText({ node: t.zone, func: (event) => texts.push(event.type === 'text' ? event.text.utf8 : event.type) });
        ui.cursor({ node: t.zone, cursor: 'text' });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
        return t;
    }
    frame(1.0, []);
    frame(1.1, [touchEvent('touchBegan', 1, [10, 10], 1.1), { type: 'scroll', scroll: { delta: [0, -3], pos: [10, 10], timestamp: 1.1 } }]);
    frame(1.2, [touchEvent('touchEnded', 1, [10, 10], 1.2)]);
    check(pointerEvents.length === 2 && pointerEvents[0] === 'touchBegan false' && eq(scrolls, [[0, -3]]), 'pointer and scroll callbacks receive their events');
    frame(1.3, [hoverEvent('hoverBegan', 2, [20, 20], 1.3)]);
    frame(1.4, []);
    check(ui.inputGetCursor() === 'text', 'cursors follow the hovered node');

    const t = frame(1.5, []);
    const key = ui.getKey(t.zone);
    ui.inputSetFocus(key);
    check(ui.inputGetFocus() === key && key > 2 ** 32, 'focus keys hold 64 bits');
    frame(1.6, [{ type: 'text', text: { utf8: 'hé' } }, { type: 'textCommand', textCommand: { command: 'moveLeft', select: true } }, { type: 'markedText', markedText: { utf8: 'm', caret: 1 } }, { type: 'key', key: { key: 'a', down: true } }]);
    check(eq(texts, ['hé', 'textCommand', 'markedText']), 'text callbacks receive text, command and marked text events while focused');
    check(ui.inputHasFocusableAt([5, 5]), 'focusable nodes answer hit queries');
    ui.inputSetFocus(0);
    frame(1.7, [{ type: 'text', text: { utf8: 'x' } }]);
    check(texts.length === 3, 'text events stop after focus clears');

    const saved = ui.inputGet();
    const other = ui.inputCreate();
    ui.inputSet(other);
    check(ui.inputGet() === other, 'the current input round trips');
    ui.inputSetCaretRect([1, 2], [3, 4]);
    const caret = ui.inputGetCaretRect();
    check(caret !== null && eq(caret.originPt, [1, 2]) && eq(caret.sizePt, [3, 4]), 'caret rects round trip');
    ui.inputPostClipboard('clip €');
    check(ui.inputTakeClipboard() === 'clip €' && ui.inputTakeClipboard() === null, 'clipboard text is taken once');
    ui.inputSetPalmRadius(12);
    check(ui.inputGetPalmRadius() === 12, 'palm radius round trips');

    let otherTaps = 0;
    const root = ui.frame({ size: ui.sizeFix(100, 100) });
    ui.tap({ node: root, onChange: (g) => { if (ui.tapGestureGetPhase(g) === 'recognized') otherTaps++; } });
    ui.layout(root);
    ui.inputDispatch(root, 2.0, [touchEvent('touchBegan', 5, [10, 10], 2.0)], 1.0);
    ui.inputDispatch(root, 2.1, [touchEvent('touchEnded', 5, [10, 10], 2.1)], 1.0);
    check(otherTaps === 1, 'another current input routes its own events');
    ui.release(root);
    ui.inputDispatch(ui.nodeNull, 2.2, [], 1.0);
    ui.inputSet(saved);
    ui.inputDestroy(other);
    check(ui.inputGet() === saved, 'destroying another input keeps the current one');
    const temp = ui.inputCreate();
    ui.inputSet(temp);
    ui.inputDestroy(temp);
    check(ui.inputGet() === 0, 'destroying the current input clears it');
    ui.inputSet(saved);

    ui.release(prev);
    dispatch(ui.nodeNull, 3.0, []);
});

js_test('lifetime', async () => {
    let prev = ui.nodeNull;
    let gesture = 0;
    const functions = { accepts: () => 'accept', onPointer: () => {}, destroy: () => {} };
    for (let i = 0; i < 300; i++) {
        const timestamp = 10 + i * 0.01;
        dispatch(prev, timestamp, i % 7 === 0 ? [touchEvent('touchBegan', 1, [5, 5], timestamp)] : i % 7 === 1 ? [touchEvent('touchEnded', 1, [5, 5], timestamp)] : []);
        const root = ui.frame({ size: ui.sizeFix(100, 100) });
        for (let k = 0; k < 10; k++) {
            const zone = ui.frame({ parent: root, key: k, size: ui.sizeFix(10, 10) });
            ui.tap({ node: zone, onChange: () => {} });
            ui.hover({ node: zone, onChange: () => {} });
            ui.onPointer({ node: zone, func: () => {} });
            ui.custom({ parent: zone, size: ui.sizeFix(1, 1), desc: { onRender: () => {} } });
        }
        gesture = ui.gesture({ node: root, name: 'probe', ...functions });
        ui.layout(root);
        for (let k = 0; k < 10; k++) {
            ui.hitTest(root, [1, 1], () => true, 4);
        }
        ui.release(prev);
        prev = root;
    }
    check(ui.gestureGetPhase(gesture) === 'possible' && ui.gestureGetName(gesture) === 'probe', 'callbacks keep working across 300 frames of redeclaration');
    ui.release(prev);
    dispatch(ui.nodeNull, 20, []);
});

function js_test_webkit_ids(first) {
    let next = first;
    return () => {
        const identifier = next;
        next = next + 1 === 0xffffffff ? 1 : next + 1;
        const pointerId = identifier | 0;
        return (pointerId >>> 0) + 1;
    };
}

js_test('browser ids', async () => {
    const nextId = js_test_webkit_ids(0xffffff40);
    const mouse = 2;
    let taps = 0;
    let presses = 0;
    let hovered = false;
    const began = [];
    const accepted = [];
    let prev = ui.nodeNull;
    let timestamp = 1.0;
    function frame(events) {
        timestamp += 0.02;
        for (const event of events) {
            event.pointer.timestamp = timestamp;
        }
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.tap({ node: t.zone, onChange: (g) => { if (ui.tapGestureGetPhase(g) === 'recognized') taps++; } });
        ui.longPress({ node: t.zone, duration: 0.1, onChange: (g) => { if (ui.longPressGestureGetPhase(g) === 'began') presses++; } });
        hovered = ui.hoverGestureHasHover(ui.hover({ node: t.zone }));
        ui.onPointer({ node: t.zone, func: (event) => { if (event.type === 'touchBegan') began.push(event.id); } });
        ui.gesture({ node: t.zone, name: 'probe', accepts: (g, event) => { if (event.type === 'touchBegan') accepted.push(event.id); return 'reject'; } });
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    frame([]);
    frame([hoverEvent('hoverBegan', mouse, [30, 30])]);
    frame([hoverEvent('hoverMoved', mouse, [31, 30])]);
    check(hovered, 'a mouse keeps hovering before the touches');

    const expected = [];
    for (let i = 0; i < 300; i++) {
        const id = nextId();
        expected.push(id);
        frame([{ type: 'pointer', pointer: { type: 'hoverBegan', source: 'direct', id: id, pos: [100, 100] } }]);
        frame([touchEvent('touchBegan', id, [100, 100])]);
        frame([touchEvent('touchMoved', id, [100, 100])]);
        frame([touchEvent('touchEnded', id, [100, 100])]);
        frame([{ type: 'pointer', pointer: { type: 'hoverEnded', source: 'direct', id: id, pos: [100, 100] } }]);
    }
    check(taps === 300, 'every tap with a fresh id recognizes, ' + taps + ' of 300');
    check(eq(began, expected) && eq(accepted, expected), 'callbacks see the ids of all 300 touches, including those above 2^31 and across the wrap');
    check(expected[0] === 0xffffff41 && expected[191] === 2 && expected[299] === 110, 'the ids follow the identifier sequence');

    for (let i = 0; i < 150; i++) {
        const id = nextId();
        const pencil = (type, pos) => ({ type: 'pointer', pointer: { type: type, source: 'pencil', id: id, pos: pos } });
        frame([pencil('hoverBegan', [100, 100])]);
        frame([pencil('touchBegan', [100, 100])]);
        frame([pencil('touchMoved', [140, 100])]);
        frame([pencil('touchEnded', [140, 100])]);
        frame([pencil('hoverEnded', [140, 100])]);
    }
    for (let i = 0; i < 150; i++) {
        const id = nextId();
        frame([{ type: 'pointer', pointer: { type: 'hoverBegan', source: 'direct', id: id, pos: [100, 100] } }]);
        frame([touchEvent('touchBegan', id, [100, 100])]);
        frame([touchEvent('canceled', id, [100, 100])]);
        frame([{ type: 'pointer', pointer: { type: 'hoverEnded', source: 'direct', id: id, pos: [100, 100] } }]);
    }
    check(began.length === 600 && accepted.length === 600, 'pencil strokes and canceled touches reach the callbacks');

    const last = nextId();
    frame([touchEvent('touchBegan', last, [100, 100])]);
    for (let i = 0; i < 6; i++) {
        frame([]);
    }
    frame([touchEvent('touchEnded', last, [100, 100])]);
    frame([hoverEvent('hoverMoved', mouse, [32, 30])]);
    check(presses === 1 && hovered, 'after 600 touches a long press still begins and the mouse still hovers');

    ui.release(prev);
    dispatch(ui.nodeNull, timestamp + 1.0, []);
});

js_test('pencil lifetime', async () => {
    const nextId = js_test_webkit_ids(0x7fffff00);
    let taps = 0;
    let hovered = false;
    let prev = ui.nodeNull;
    let timestamp = 1.0;
    function frame(events) {
        timestamp += 0.02;
        for (const event of events) {
            event.pointer.timestamp = timestamp;
        }
        dispatch(prev, timestamp, events);
        const t = js_test_root();
        ui.tap({ node: t.zone, onChange: (g) => { if (ui.tapGestureGetPhase(g) === 'recognized') taps++; } });
        hovered = ui.hoverGestureHasHover(ui.hover({ node: t.zone }));
        ui.layout(t.root);
        ui.release(prev);
        prev = t.root;
    }
    const pencil = (type, id, pos) => ({ type: 'pointer', pointer: { type: type, source: 'pencil', id: id, pos: pos } });
    frame([]);
    for (let i = 0; i < 200; i++) {
        const id = nextId();
        frame([pencil('hoverBegan', id, [100, 100])]);
        frame([pencil('touchBegan', id, [100, 100])]);
        frame([pencil('touchEnded', id, [100, 100])]);
    }
    frame([]);
    check(taps === 200, 'pencil touches whose ids never return all tap, ' + taps + ' of 200');
    check(hovered === false, 'a lifted pencil does not keep hovering where it ended');

    const finger = nextId();
    frame([touchEvent('touchBegan', finger, [100, 100])]);
    frame([touchEvent('touchEnded', finger, [100, 100])]);
    check(taps === 201, 'a finger still taps after 200 pencil touches');

    for (let i = 0; i < 200; i++) {
        frame([hoverEvent('hoverBegan', nextId(), [50, 50])]);
    }
    check(hovered, 'hover pointers that never end keep hovering');
    const after = nextId();
    frame([touchEvent('touchBegan', after, [100, 100])]);
    frame([touchEvent('touchEnded', after, [100, 100])]);
    check(taps === 202, 'a touch still registers after 200 hover pointers that never ended');

    ui.release(prev);
    dispatch(ui.nodeNull, timestamp + 1.0, []);
});
