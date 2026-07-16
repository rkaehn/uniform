'use strict';

const js_native = __js_native;
const js_native_timers = new Map();
let js_native_timer_next = 1;

function js_native_format(args) {
    let line = '';
    for (let i = 0; i < args.length; i++) {
        const arg = args[i];
        let text;
        if (arg instanceof Error) {
            text = arg.name + ': ' + arg.message + (arg.stack ? '\n' + arg.stack : '');
        } else if (typeof arg === 'object' && arg !== null) {
            try {
                text = JSON.stringify(arg);
            } catch (error) {
                text = String(arg);
            }
        } else {
            text = String(arg);
        }
        line += (i > 0 ? ' ' : '') + text;
    }
    return line + '\n';
}

function js_native_set_timer(fn, ms, args, repeat) {
    const id = js_native_timer_next++;
    const handle = js_native.setTimer(id, Math.max(Number(ms) || 0, 0), repeat);
    js_native_timers.set(id, { fn: fn, args: args, repeat: repeat, handle: handle });
    return id;
}

function js_native_clear_timer(id) {
    const timer = js_native_timers.get(id);
    if (timer !== undefined) {
        js_native_timers.delete(id);
        js_native.clearTimer(timer.handle);
    }
}

function js_native_response(status, buffer) {
    const text = () => Promise.resolve(js_native.decode(new Uint8Array(buffer)));
    return {
        ok: status >= 200 && status < 300,
        status: status,
        arrayBuffer: () => Promise.resolve(buffer),
        text: text,
        json: () => text().then(JSON.parse),
    };
}

class TextEncoder {
    get encoding() {
        return 'utf-8';
    }

    encode(input = '') {
        return js_native.encode(String(input));
    }
}

class TextDecoder {
    constructor(label = 'utf-8') {
        if (/^utf-?8$/i.test(label) === false) {
            throw new RangeError('TextDecoder: only utf-8 is supported');
        }
    }

    get encoding() {
        return 'utf-8';
    }

    decode(input = new Uint8Array(0)) {
        return js_native.decode(ArrayBuffer.isView(input) ? new Uint8Array(input.buffer, input.byteOffset, input.byteLength) : new Uint8Array(input));
    }
}

globalThis.console = {
    log: (...args) => js_native.print(js_native_format(args), false),
    info: (...args) => js_native.print(js_native_format(args), false),
    debug: (...args) => js_native.print(js_native_format(args), false),
    warn: (...args) => js_native.print(js_native_format(args), true),
    error: (...args) => js_native.print(js_native_format(args), true),
};
globalThis.setTimeout = (fn, ms, ...args) => js_native_set_timer(fn, ms, args, false);
globalThis.setInterval = (fn, ms, ...args) => js_native_set_timer(fn, ms, args, true);
globalThis.clearTimeout = js_native_clear_timer;
globalThis.clearInterval = js_native_clear_timer;
globalThis.queueMicrotask = (fn) => js_observe(Promise.resolve().then(fn));
globalThis.performance = { now: js_native.now };
globalThis.TextEncoder = TextEncoder;
globalThis.TextDecoder = TextDecoder;
globalThis.fetch = (url) => new Promise((resolve, reject) => {
    js_native.fetch(String(url), (status, buffer, error) => {
        if (status === 0) {
            reject(new TypeError('fetch ' + url + ': ' + error));
        } else {
            resolve(js_native_response(status, buffer));
        }
    });
});

js_native.onTimer = (id) => {
    const timer = js_native_timers.get(id);
    if (timer.repeat === false) {
        js_native_timers.delete(id);
    }
    js_protect(timer.fn, timer.args);
};

js_native.dispatch = () => {
    const a = js_layout.dispatchArgs;
    js_f64[js_layout.dispatchResult] = js_dispatch(js_f64[a], js_f64[a + 1], js_f64[a + 2], js_f64[a + 3], js_f64[a + 4], js_f64[a + 5], js_f64[a + 6], js_f64[a + 7]);
};

uniform.args = js_native.args;
uniform.exit = (code = 0) => js_native.exit(code);

js_start({
    exports: js_native.exports,
    buffer: () => js_native.buffer,
    offset: 0,
    base: js_native.base,
    arena: js_native.base,
    encode: js_native.encode,
    decode: js_native.decode,
    observe: (promise) => promise.then(undefined, js_report),
});
