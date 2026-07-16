'use strict';

js_test('web pointers', async () => {
    if (typeof document === 'undefined') {
        return;
    }
    const state = js_test_state;
    const canvas = document.getElementById('canvas' + app.windowGetNative(state.window));
    const rect = canvas.getBoundingClientRect();
    const size = app.windowGetSize(state.window);
    let taps = 0;
    let hovered = false;
    const began = [];
    let prev = ui.nodeNull;
    function frame() {
        ui.utilWindowSync(state.window, prev, 1.0);
        const root = ui.frame({ size: ui.sizeFix(size[0], size[1]) });
        ui.tap({ node: root, onChange: (g) => { if (ui.tapGestureGetPhase(g) === 'recognized') taps++; } });
        hovered = ui.hoverGestureHasHover(ui.hover({ node: root }));
        ui.onPointer({ node: root, func: (event) => { if (event.type === 'touchBegan') began.push(event.id); } });
        ui.layout(root);
        ui.release(prev);
        prev = root;
    }
    function pointer(type, pointerId, pointerType, pos, buttons) {
        canvas.dispatchEvent(new PointerEvent(type, {
            pointerId: pointerId,
            pointerType: pointerType,
            isPrimary: true,
            clientX: rect.left + pos[0],
            clientY: rect.top + pos[1],
            buttons: buttons,
            button: 0,
            pressure: buttons ? 0.5 : 0,
            width: 24,
            height: 24,
            bubbles: true,
            cancelable: true,
        }));
    }
    frame();
    frame();
    pointer('pointerenter', 1, 'mouse', [20, 20], 0);
    pointer('pointermove', 1, 'mouse', [21, 20], 0);
    frame();
    check(hovered, 'a mouse pointer hovers through the canvas listeners');

    let identifier = 0x7ffffff0;
    const expected = [];
    for (let i = 0; i < 160; i++) {
        const pointerId = identifier++ | 0;
        expected.push((pointerId >>> 0) + 1);
        pointer('pointerenter', pointerId, 'touch', [200, 200], 0);
        pointer('pointerdown', pointerId, 'touch', [200, 200], 1);
        pointer('pointermove', pointerId, 'touch', [200, 200], 1);
        pointer('pointerup', pointerId, 'touch', [200, 200], 0);
        pointer('pointerleave', pointerId, 'touch', [200, 200], 0);
        frame();
    }
    check(taps === 160, 'every touch with a Safari style id taps, ' + taps + ' of 160');
    check(eq(began, expected), 'pointer callbacks receive the ids of touches with negative pointer ids');
    check(expected[15] === 2147483648 && expected[16] === 2147483649, 'ids continue across the sign flip of the pointer id');

    for (let i = 0; i < 150; i++) {
        const pointerId = identifier++ | 0;
        pointer('pointermove', 1, 'pen', [250, 200], 0);
        pointer('pointerenter', pointerId, 'pen', [250, 200], 0);
        pointer('pointerdown', pointerId, 'pen', [250, 200], 1);
        pointer('pointerup', pointerId, 'pen', [250, 200], 0);
        if (i % 2 === 0) {
            pointer('pointerleave', pointerId, 'pen', [250, 200], 0);
        }
        frame();
    }
    check(taps === 310, 'pencil touches tap whether or not their pointer leaves, ' + taps + ' of 310');
    const finger = identifier++ | 0;
    pointer('pointerdown', finger, 'touch', [200, 200], 1);
    pointer('pointerup', finger, 'touch', [200, 200], 0);
    frame();
    check(taps === 311, 'a finger still taps after the pencil touches');

    pointer('pointermove', 0, 'mouse', [22, 20], 0);
    pointer('pointermove', 1, 'mouse', [23, 20], 0);
    frame();
    check(hovered, 'the mouse still hovers after the touches');
    pointer('pointerleave', 0, 'mouse', [22, 20], 0);
    pointer('pointerleave', 1, 'mouse', [23, 20], 0);
    frame();
    check(hovered === false, 'the hover ends with the mouse');

    ui.release(prev);
    ui.utilWindowSync(state.window, ui.nodeNull, 1.0);
});
