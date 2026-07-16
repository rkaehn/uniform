'use strict';

let js_host = null;
let js_x = null;
let js_base = 0;
let js_arena = 0;
let js_u8 = null;
let js_i8 = null;
let js_u16 = null;
let js_i16 = null;
let js_u32 = null;
let js_i32 = null;
let js_f32 = null;
let js_f64 = null;
let js_scratch_base = 0;
let js_scratch_cursor = 0;
let js_scratch_end = 0;
let js_root_slot = 0;
let js_last_len = 0;
let js_mem_libc = 0;

const js_kind_shifts = { f64: 3, f32: 2, u32: 2, i32: 2, u16: 1, i16: 1, u8: 0, i8: 0, bool: 0, u64: 3, i64: 3 };

function js_error(message) {
    const error = new Error('uniform: ' + message);
    error.check = true;
    return error;
}

function js_assert(condition, message) {
    if (!condition) {
        throw js_error(message);
    }
}

function js_num(v) {
    return v == null ? 0 : v;
}

function js_views_set() {
    const buffer = js_host.buffer();
    js_u8 = new Uint8Array(buffer, js_host.offset);
    js_i8 = new Int8Array(buffer, js_host.offset);
    js_u16 = new Uint16Array(buffer, js_host.offset);
    js_i16 = new Int16Array(buffer, js_host.offset);
    js_u32 = new Uint32Array(buffer, js_host.offset);
    js_i32 = new Int32Array(buffer, js_host.offset);
    js_f32 = new Float32Array(buffer, js_host.offset);
    js_f64 = new Float64Array(buffer, js_host.offset);
}

function js_views_check() {
    if (js_u8.buffer !== js_host.buffer()) {
        js_views_set();
    }
}

function js_kind_view(kind) {
    switch (kind) {
    case 'f64': return js_f64;
    case 'f32': return js_f32;
    case 'u32': return js_u32;
    case 'i32': return js_i32;
    case 'u16': return js_u16;
    case 'i16': return js_i16;
    case 'i8': return js_i8;
    default: return js_u8;
    }
}

function js_enum(table, value) {
    const result = table[value];
    if (result !== undefined) {
        return result;
    }
    if (typeof value === 'number') {
        return value;
    }
    if (value == null) {
        return 0;
    }
    throw js_error('unknown enum value ' + String(value));
}

function js_enum_name(names, value) {
    const name = names[value];
    return name === undefined ? value : name;
}

function js_w64(o, v) {
    const n = js_num(v);
    js_assert(Number.isInteger(n) && n >= 0 && n <= 9007199254740991, 'a 64 bit value must be a non-negative integer below 2^53');
    js_u32[o >> 2] = n % 4294967296;
    js_u32[(o >> 2) + 1] = Math.floor(n / 4294967296);
}

function js_r64(o) {
    const hi = js_u32[(o >> 2) + 1];
    js_assert(hi < 2097152, 'a 64 bit value from the library does not fit in 53 bits');
    return hi * 4294967296 + js_u32[o >> 2];
}

function js_scratch(size) {
    const off = js_scratch_cursor;
    const next = off + ((size + 15) & ~15);
    if (next > js_scratch_end) {
        throw js_error('scratch overflow: ' + size + ' bytes requested with ' + (js_scratch_end - off) + ' free; pass large data through uniform.alloc');
    }
    js_scratch_cursor = next;
    return off;
}

function js_in_view(ptr, len) {
    return ptr >= js_base && ptr + len <= js_base + js_u8.length;
}

function js_pstr(str) {
    if (str == null) {
        js_last_len = 0;
        return 0;
    }
    if (typeof str !== 'string') {
        str = String(str);
    }
    const off = js_scratch(str.length + 1);
    let i = 0;
    for (; i < str.length; i++) {
        const c = str.charCodeAt(i);
        if (c >= 0x80) {
            break;
        }
        js_u8[off + i] = c;
    }
    if (i < str.length) {
        const bytes = js_host.encode(str);
        js_scratch_cursor = off;
        js_scratch(bytes.length + 1);
        js_u8.set(bytes, off);
        i = bytes.length;
    }
    js_u8[off + i] = 0;
    js_last_len = i;
    return js_base + off;
}

function js_bytes_view(bytes) {
    if (bytes instanceof Uint8Array) {
        return bytes;
    }
    if (bytes instanceof ArrayBuffer) {
        return new Uint8Array(bytes);
    }
    js_assert(ArrayBuffer.isView(bytes), 'expected a typed array or an ArrayBuffer');
    return new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

function js_pbytes(bytes) {
    if (bytes == null) {
        js_last_len = 0;
        return 0;
    }
    const u8 = js_bytes_view(bytes);
    const off = js_scratch(u8.length);
    js_u8.set(u8, off);
    js_last_len = u8.length;
    return js_base + off;
}

function js_read_str(off, len) {
    if (len <= 64) {
        let s = '';
        for (let i = 0; i < len; i++) {
            const c = js_u8[off + i];
            if (c >= 0x80) {
                return js_host.decode(js_u8.subarray(off, off + len));
            }
            s += String.fromCharCode(c);
        }
        return s;
    }
    return js_host.decode(js_u8.subarray(off, off + len));
}

function js_wchars(o, str, n) {
    const bytes = js_host.encode(String(str));
    const len = Math.min(bytes.length, n - 1);
    js_u8.set(bytes.subarray(0, len), o);
    js_u8[o + len] = 0;
}

function js_rchars(o, n) {
    let len = 0;
    while (len < n && js_u8[o + len] !== 0) {
        len++;
    }
    return js_read_str(o, len);
}

function js_ra_chars(o, count, stride, n) {
    const result = [];
    for (let i = 0; i < count; i++) {
        result.push(js_rchars(o + i * stride, n));
    }
    return result;
}

function js_pa_num(a, kind) {
    if (a == null || a.length === 0) {
        js_last_len = 0;
        return 0;
    }
    const n = a.length;
    const shift = js_kind_shifts[kind];
    const off = js_scratch(n << shift);
    if (kind === 'u64' || kind === 'i64') {
        for (let i = 0; i < n; i++) js_w64(off + i * 8, a[i]);
    } else if (kind === 'bool') {
        for (let i = 0; i < n; i++) js_u8[off + i] = a[i] ? 1 : 0;
    } else {
        const view = js_kind_view(kind);
        for (let i = 0; i < n; i++) view[(off >> shift) + i] = a[i];
    }
    js_last_len = n;
    return js_base + off;
}

function js_ra_num(off, n, kind) {
    const result = [];
    if (kind === 'u64' || kind === 'i64') {
        for (let i = 0; i < n; i++) result.push(js_r64(off + i * 8));
    } else if (kind === 'bool') {
        for (let i = 0; i < n; i++) result.push(js_u8[off + i] !== 0);
    } else {
        const shift = js_kind_shifts[kind];
        const view = js_kind_view(kind);
        for (let i = 0; i < n; i++) result.push(view[(off >> shift) + i]);
    }
    return result;
}

function js_pa_vec(a, kind, lanes) {
    if (a == null || a.length === 0) {
        js_last_len = 0;
        return 0;
    }
    const shift = js_kind_shifts[kind];
    const view = js_kind_view(kind);
    const stride = (lanes === 3 ? 4 : lanes) << shift;
    const off = js_scratch(stride * a.length);
    for (let i = 0; i < a.length; i++) {
        for (let c = 0; c < lanes; c++) view[((off + i * stride) >> shift) + c] = a[i][c];
    }
    js_last_len = a.length;
    return js_base + off;
}

function js_ra_vec(off, n, kind, lanes) {
    const shift = js_kind_shifts[kind];
    const view = js_kind_view(kind);
    const stride = (lanes === 3 ? 4 : lanes) << shift;
    const result = [];
    for (let i = 0; i < n; i++) {
        const v = [];
        for (let c = 0; c < lanes; c++) v.push(view[((off + i * stride) >> shift) + c]);
        result.push(v);
    }
    return result;
}

function js_pa_enum(a, table) {
    if (a == null || a.length === 0) {
        js_last_len = 0;
        return 0;
    }
    const off = js_scratch(4 * a.length);
    for (let i = 0; i < a.length; i++) js_u32[(off >> 2) + i] = js_enum(table, a[i]);
    js_last_len = a.length;
    return js_base + off;
}

function js_ra_enum(off, n, names) {
    const result = [];
    for (let i = 0; i < n; i++) result.push(js_enum_name(names, js_u32[(off >> 2) + i]));
    return result;
}

function js_pa_ptr(a) {
    if (a == null || a.length === 0) {
        js_last_len = 0;
        return 0;
    }
    const off = js_scratch(js_ptr_size * a.length);
    for (let i = 0; i < a.length; i++) js_wp(off + i * js_ptr_size, a[i]);
    js_last_len = a.length;
    return js_base + off;
}

function js_ra_ptr(off, n) {
    const result = [];
    for (let i = 0; i < n; i++) result.push(js_rp(off + i * js_ptr_size));
    return result;
}

function js_pa_str(a) {
    return js_pa_ptr(a == null ? a : a.map(js_pstr));
}

function js_ra_str(off, n) {
    return js_ra_ptr(off, n).map(js_host_str);
}

function js_pa_bytes(a) {
    return js_pa_ptr(a == null ? a : a.map(js_pbytes));
}

function js_host_copy(ptr, len) {
    if (len === 0 || ptr === 0) {
        return js_scratch(0);
    }
    if (js_in_view(ptr, len) === false) {
        js_views_check();
    }
    if (js_in_view(ptr, len)) {
        return ptr - js_base;
    }
    const off = js_scratch(len);
    js_x.js_memcpy(js_base + off, ptr, len);
    return off;
}

function js_host_read(ptr, len) {
    const result = new Uint8Array(len);
    const m = js_scratch_cursor;
    for (let done = 0; done < len;) {
        const chunk = Math.min(len - done, js_scratch_end - js_scratch_cursor);
        const off = js_host_copy(ptr + done, chunk);
        result.set(js_u8.subarray(off, off + chunk), done);
        js_scratch_cursor = m;
        done += chunk;
    }
    return result;
}

function js_host_write(ptr, bytes) {
    const u8 = js_bytes_view(bytes);
    if (js_in_view(ptr, u8.length)) {
        js_u8.set(u8, ptr - js_base);
        return;
    }
    const m = js_scratch_cursor;
    for (let done = 0; done < u8.length;) {
        const chunk = Math.min(u8.length - done, js_scratch_end - js_scratch_cursor);
        const off = js_scratch(chunk);
        js_u8.set(u8.subarray(done, done + chunk), off);
        js_x.js_memcpy(ptr + done, js_base + off, chunk);
        js_scratch_cursor = m;
        done += chunk;
    }
}

function js_host_str_n(ptr, len) {
    if (ptr === 0 || len === 0) {
        return '';
    }
    const m = js_scratch_cursor;
    const result = js_read_str(js_host_copy(ptr, len), len);
    js_scratch_cursor = m;
    return result;
}

function js_host_str(ptr) {
    return ptr === 0 ? '' : js_host_str_n(ptr, js_x.js_strlen(ptr));
}

const js_key_explicit = 4503599627370496;

function js_w_ui_key(o, key) {
    if (key == null) {
        return;
    }
    if (typeof key === 'string') {
        js_assert(key.length > 0, 'a string key must not be empty; leave the key out for an automatic key');
        const m = js_scratch_cursor;
        js_x.ui_key_str(js_pstr(key), js_base + o);
        js_scratch_cursor = m;
        return;
    }
    js_assert(typeof key === 'number', 'a key is a string or a number');
    js_w64(o, key % js_key_explicit + js_key_explicit);
}

function js_r_ui_key(o) {
    const raw = js_r64(o);
    return raw === 0 ? undefined : raw;
}

function js_pone_ui_key(key) {
    const p = js_scratch(8);
    js_u8.fill(0, p, p + 8);
    js_w_ui_key(p, key);
    return js_base + p;
}

function js_pa_ui_key(a) {
    if (a == null || a.length === 0) {
        js_last_len = 0;
        return 0;
    }
    const p = js_scratch(8 * a.length);
    js_u8.fill(0, p, p + 8 * a.length);
    for (let i = 0; i < a.length; i++) js_w_ui_key(p + i * 8, a[i]);
    js_last_len = a.length;
    return js_base + p;
}

function js_ra_ui_key(o, n) {
    const result = [];
    for (let i = 0; i < n; i++) result.push(js_r_ui_key(o + i * 8));
    return result;
}

const js_cb_holders = [null];
const js_cb_gens = [0];
const js_cb_free = [];
const js_cb_calls = [];
const js_cb_pending = [];
const js_cb_trees = new Map();
const js_cb_retired = new Map();
const js_cb_replaced = new Map();
const js_cb_owners = new Map();
let js_cb_owner_pending = null;
const js_cb_addrs = new Array(js_site_count).fill(0);
let js_input_current = 0;

function js_cb_addr(kind) {
    let addr = js_cb_addrs[kind];
    if (addr === 0) {
        addr = js_x.js_cb_addr(kind);
        js_cb_addrs[kind] = addr;
    }
    return addr;
}

function js_cb_register(holder) {
    if (holder == null) {
        return 0;
    }
    let index;
    if (js_cb_free.length > 0) {
        index = js_cb_free.pop();
    } else {
        index = js_cb_holders.length;
        js_assert(index < 1048576, 'too many live callbacks');
        js_cb_holders.push(null);
        js_cb_gens.push(0);
    }
    js_cb_holders[index] = holder;
    return ((js_cb_gens[index] << 20) | index) >>> 0;
}

function js_cb_index(id) {
    const index = id & 0xfffff;
    return index < js_cb_holders.length && js_cb_gens[index] === id >>> 20 ? index : 0;
}

function js_cb_get(id) {
    return js_cb_holders[js_cb_index(id)];
}

function js_cb_release(id) {
    const index = js_cb_index(id);
    if (index === 0 || js_cb_holders[index] === null) {
        return;
    }
    js_cb_holders[index] = null;
    js_cb_gens[index] = (js_cb_gens[index] + 1) & 0xfff;
    js_cb_free.push(index);
}

function js_cb_call(holder) {
    const id = js_cb_register(holder);
    if (id !== 0) {
        js_cb_calls.push(id);
    }
    return id;
}

function js_cb_call_release(mark) {
    while (js_cb_calls.length > mark) {
        js_cb_release(js_cb_calls.pop());
    }
}

function js_cb_pending_release(mark) {
    for (let i = mark; i < js_cb_pending.length; i += 2) {
        js_cb_release(js_cb_pending[i]);
    }
    js_cb_pending.length = mark;
}

function js_cb_tree(holder) {
    const id = js_cb_register(holder);
    if (id !== 0) {
        js_cb_pending.push(id, 0);
    }
    return id;
}

function js_cb_input(holder) {
    const id = js_cb_register(holder);
    if (id !== 0) {
        js_cb_pending.push(id, js_input_current);
    }
    return id;
}

function js_cb_adopt(mark, root) {
    if (js_cb_pending.length === mark) {
        return;
    }
    let entries = js_cb_trees.get(root);
    if (entries === undefined) {
        entries = [];
        js_cb_trees.set(root, entries);
    }
    for (let i = mark; i < js_cb_pending.length; i++) {
        entries.push(js_cb_pending[i]);
    }
    js_cb_pending.length = mark;
}

function js_cb_retire(input, id) {
    let ids = js_cb_retired.get(input);
    if (ids === undefined) {
        ids = [];
        js_cb_retired.set(input, ids);
    }
    ids.push(id);
}

function js_cb_release_tree(root) {
    const entries = js_cb_trees.get(root);
    if (entries === undefined) {
        return;
    }
    js_cb_trees.delete(root);
    for (let i = 0; i < entries.length; i += 2) {
        if (entries[i + 1] === 0) {
            js_cb_release(entries[i]);
        } else {
            js_cb_retire(entries[i + 1], entries[i]);
        }
    }
}

function js_cb_sweep(input) {
    const ids = js_cb_retired.get(input);
    if (ids === undefined) {
        return;
    }
    js_cb_retired.delete(input);
    for (let i = 0; i < ids.length; i++) {
        js_cb_release(ids[i]);
    }
}

function js_cb_release_input(input) {
    js_cb_sweep(input);
    for (const entries of js_cb_trees.values()) {
        for (let i = 0; i < entries.length; i += 2) {
            if (entries[i + 1] === input) {
                js_cb_release(entries[i]);
                entries[i + 1] = 0;
            }
        }
    }
}

function js_cb_replace_release(key) {
    const previous = js_cb_replaced.get(key);
    if (previous !== undefined) {
        js_cb_release(previous);
        js_cb_replaced.delete(key);
    }
}

function js_cb_replace(key, holder) {
    js_cb_replace_release(key);
    const id = js_cb_register(holder);
    if (id !== 0) {
        js_cb_replaced.set(key, id);
    }
    return id;
}

function js_cb_own(owner, holder) {
    js_cb_owners.set(owner, holder);
}

function js_cb_disown(owner) {
    js_cb_owners.delete(owner);
}

function js_cb_owner(owner) {
    let holder = js_cb_owners.get(owner);
    if (holder === undefined && js_cb_owner_pending !== null) {
        holder = js_cb_owner_pending;
        js_cb_owners.set(owner, holder);
    }
    return holder === undefined ? null : holder;
}

function js_observe(result) {
    if (result instanceof Promise && js_host.observe !== undefined) {
        js_host.observe(result);
    }
}

const js_gpu_pending = [];
let js_app_handle = 0;
let js_app_desc = null;
let js_restart_timer = 0;
let js_report_text = '';
let js_report_time = -Infinity;

function js_restart() {
    js_restart_timer = 0;
    app.restart(js_app_handle, js_app_desc);
}

function js_report(error) {
    const text = error instanceof Error ? error.name + ': ' + error.message + '\n' + error.stack : String(error);
    const now = performance.now();
    if (text !== js_report_text || now - js_report_time > 1000) {
        js_report_text = text;
        js_report_time = now;
        console.error(error);
    }
    if (error instanceof Error && error.check === true && js_restart_timer === 0 && js_app_handle !== 0) {
        js_restart_timer = setTimeout(js_restart, 1000);
    }
}

function js_recover(error, scratch, calls, pending, gpu) {
    js_scratch_cursor = scratch;
    js_cb_call_release(calls);
    js_cb_pending_release(pending);
    while (js_gpu_pending.length > gpu) {
        js_x.gpu_command_buffer_discard(js_gpu_pending.pop());
    }
    js_report(error);
}

function js_protect(fn, args) {
    const scratch = js_scratch_cursor;
    const calls = js_cb_calls.length;
    const pending = js_cb_pending.length;
    const gpu = js_gpu_pending.length;
    try {
        js_observe(fn(...args));
    } catch (error) {
        js_recover(error, scratch, calls, pending, gpu);
    }
}

function js_dispatch(kind, id, a0, a1, a2, a3, a4, a5) {
    js_views_check();
    const scratch = js_scratch_cursor;
    const calls = js_cb_calls.length;
    const pending = js_cb_pending.length;
    const gpu = js_gpu_pending.length;
    try {
        return js_dispatch_gen(kind, id, a0, a1, a2, a3, a4, a5);
    } catch (error) {
        js_recover(error, scratch, calls, pending, gpu);
        return 0;
    }
}

function js_start(host) {
    js_host = host;
    js_x = host.exports;
    js_base = host.base;
    js_arena = host.arena - host.base;
    js_views_set();
    js_scratch_base = js_arena + js_layout.argsSize;
    js_scratch_cursor = js_scratch_base;
    js_scratch_end = js_arena + js_layout.scratchSize;
    js_root_slot = (js_arena + js_layout.rootSlot * 8) >> 3;
    js_mem_libc = js_x.js_mem_libc();
    js_install_platform();
}
