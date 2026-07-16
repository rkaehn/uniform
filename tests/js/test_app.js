'use strict';

js_test('app', async () => {
    const state = js_test_state;
    await new Promise((resolve) => setTimeout(resolve, 0));
    const size = app.windowGetSize(state.window);
    const windowDesc = {
        onUpdate: js_test_on_update,
        onCloseRequest: () => uniform.exit(1),
        title: 'uniform js tests',
        size: [320, 200],
    };
    let starts = 0;
    let quits = 0;
    const desc = {
        onStart: (a) => {
            starts++;
            state.window = app.windowCreate(a, windowDesc);
        },
        onQuit: () => quits++,
    };
    app.restart(state.app, desc);
    check(starts === 1 && quits === 0 && state.window !== 0, 'restart starts the new description');
    check(eq(app.windowGetSize(state.window), size), 'a window of the restarted app takes the place of the closed one');
    app.restart(state.app, desc);
    check(starts === 2 && quits === 1, 'restart quits the previous description first');

    ui.deinit();
    ui.init(state.device);
    state.font = ui.fontCreate(state.fontData);
    ui.setDefaultFont(state.font);
    const frame = ui.frame({ size: ui.sizeFix(32, 32), layout: ui.layoutStackX('center', 'center', 0, 0), fill: ui.fillSolid(ui.black) });
    ui.text({ parent: frame, str: ui.str('W', ui.white, 24) });
    const pixel = await js_test_render(frame, 32, 32);
    let lit = false;
    for (let y = 0; y < 32; y++) {
        for (let x = 0; x < 32; x++) {
            lit = lit || pixel(x, y)[1] > 128;
        }
    }
    check(lit, 'fonts are released with the context that owned them');
    ui.release(frame);
});
