'use strict';

const web_main = is_worker ? null : document.currentScript.getAttribute('data-main');
const web_set_timeout = setTimeout;
const web_set_interval = setInterval;

function web_format(args) {
    let line = '';
    for (let i = 0; i < args.length; i++) {
        const arg = args[i];
        let text;
        if (arg instanceof Error) {
            const head = arg.name + ': ' + arg.message;
            text = arg.stack ? (arg.stack.startsWith(head) ? arg.stack : head + '\n' + arg.stack) : head;
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
    return line;
}

function web_console() {
    for (const level of ['log', 'info', 'debug', 'warn', 'error']) {
        const native = console[level].bind(console);
        console[level] = (...args) => {
            native(...args);
            remote_log_send(web_format(args));
        };
    }
}

function web_report(error) {
    js_recover(error, js_scratch_base, 0, 0, js_gpu_pending.length);
}

function web_exit(code = 0) {
    remote_log_chain = remote_log_chain.then(() => fetch(new URL('/exit?' + code, base_url), { cache: 'no-store' })).catch(() => {});
}

function web_start() {
    wasm_exports.js_arena_init();
    js_start({
        exports: wasm_exports,
        buffer: () => wasm_memory.buffer,
        offset: 0,
        base: 0,
        arena: wasm_exports.js_arena_base(),
        encode: (str) => text_encoder.encode(str),
        decode: (u8) => text_decoder.decode(u8.slice()),
    });
    const script = document.createElement('script');
    script.src = web_main;
    script.onerror = () => fatal(new Error('cannot load ' + web_main));
    document.body.appendChild(script);
}

env.js_dispatch = js_dispatch;
uniform.args = [web_main].concat(boot_args());
uniform.exit = web_exit;
if (is_worker === false) {
    uncaught = web_report;
    globalThis.setTimeout = (fn, ms, ...args) => web_set_timeout(js_protect, ms, fn, args);
    globalThis.setInterval = (fn, ms, ...args) => web_set_interval(js_protect, ms, fn, args);
    if (remote_log) {
        web_console();
    }
    const preload = document.createElement('link');
    preload.rel = 'preload';
    preload.as = 'script';
    preload.href = web_main;
    document.head.appendChild(preload);
}
