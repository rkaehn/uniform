'use strict';

const is_worker = typeof window === 'undefined';
const script_url = is_worker ? null : document.currentScript.src;
let base_url = is_worker ? null : location.href;
const wasm_url = is_worker ? null : new URL('index.wasm', script_url);

let wasm_module = null;
let wasm_exports = null;
let wasm_memory = null;
let wasm_table = null;
let heap_u8 = new Uint8Array(0);
let time_origin = performance.timeOrigin;
let thread_next_id = 1;
const thread_count = navigator.hardwareConcurrency || 4;

function now_ms() {
    return performance.now() + performance.timeOrigin - time_origin;
}

function mu8() {
    if (heap_u8.buffer !== wasm_memory.buffer) {
        heap_u8 = new Uint8Array(wasm_memory.buffer);
    }
    return heap_u8;
}

function ru32(ptr) {
    const m = mu8();
    return (m[ptr] | (m[ptr + 1] << 8) | (m[ptr + 2] << 16) | (m[ptr + 3] << 24)) >>> 0;
}

function ri32(ptr) {
    return ru32(ptr) | 0;
}

function rf32(ptr) {
    return new DataView(wasm_memory.buffer).getFloat32(ptr, true);
}

const text_decoder = new TextDecoder();
const text_encoder = new TextEncoder();

function cstr(ptr) {
    const m = mu8();
    let end = m.indexOf(0, ptr);
    if (end < 0) {
        end = m.length;
    }
    return text_decoder.decode(m.slice(ptr, end));
}

function cstr_alloc(str) {
    const bytes = text_encoder.encode(str);
    const ptr = wasm_exports.malloc(bytes.length + 1);
    mu8().set(bytes, ptr);
    mu8()[ptr + bytes.length] = 0;
    return ptr;
}

function with_cstr(str, func) {
    const ptr = cstr_alloc(str);
    func(ptr);
    wasm_exports.free(ptr);
}

const stdio_buffers = { 1: '', 2: '' };

let remote_log = !is_worker && location.search.indexOf('--remote-log') >= 0;
let remote_log_chain = Promise.resolve();

function remote_log_send(line) {
    if (remote_log) {
        remote_log_chain = remote_log_chain.then(() => fetch(new URL('/log?' + encodeURIComponent(line), base_url), { cache: 'no-store' })).catch(() => {});
    }
}

function stdio_write(fd, str) {
    let buffer = stdio_buffers[fd] + str;
    let newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const line = buffer.slice(0, newline);
        if (fd === 2) {
            console.error(line);
        } else {
            console.log(line);
        }
        remote_log_send(line);
        buffer = buffer.slice(newline + 1);
    }
    stdio_buffers[fd] = buffer;
}

function fatal(error) {
    stdio_write(2, 'fatal: ' + (error && error.stack ? error.stack : String(error)) + '\n');
}

let uncaught = fatal;

const wasi = {
    fd_write(fd, iovs, iovs_len, nwritten_ptr) {
        let written = 0;
        let str = '';
        for (let i = 0; i < iovs_len; i++) {
            const ptr = ru32(iovs + i * 8);
            const len = ru32(iovs + i * 8 + 4);
            str += text_decoder.decode(mu8().slice(ptr, ptr + len));
            written += len;
        }
        if (fd === 1 || fd === 2) {
            stdio_write(fd, str);
        }
        mu8().set([written & 0xff, (written >> 8) & 0xff, (written >> 16) & 0xff, (written >> 24) & 0xff], nwritten_ptr);
        return 0;
    },
    fd_read() {
        return 8;
    },
    fd_close() {
        return 0;
    },
    fd_seek() {
        return 8;
    },
    fd_fdstat_get(fd, stat_ptr) {
        mu8().fill(0, stat_ptr, stat_ptr + 24);
        mu8()[stat_ptr] = 2;
        return 0;
    },
    fd_fdstat_set_flags() {
        return 0;
    },
    fd_prestat_get() {
        return 8;
    },
    fd_prestat_dir_name() {
        return 8;
    },
    path_open() {
        return 44;
    },
    environ_sizes_get(count_ptr, size_ptr) {
        mu8().fill(0, count_ptr, count_ptr + 4);
        mu8().fill(0, size_ptr, size_ptr + 4);
        return 0;
    },
    environ_get() {
        return 0;
    },
    clock_time_get(clock_id, precision, time_ptr) {
        const ms = clock_id === 0 ? Date.now() : now_ms();
        new DataView(wasm_memory.buffer).setBigUint64(time_ptr, BigInt(Math.round(ms * 1e6)), true);
        return 0;
    },
    clock_res_get(clock_id, res_ptr) {
        new DataView(wasm_memory.buffer).setBigUint64(res_ptr, 1000000n, true);
        return 0;
    },
    random_get(ptr, len) {
        const temp = new Uint8Array(Math.min(len, 65536));
        for (let offset = 0; offset < len; offset += 65536) {
            const n = Math.min(65536, len - offset);
            crypto.getRandomValues(temp.subarray(0, n));
            mu8().set(temp.subarray(0, n), ptr + offset);
        }
        return 0;
    },
    sched_yield() {
        return 0;
    },
    poll_oneoff() {
        return 52;
    },
    proc_exit(code) {
        if (remote_log) {
            remote_log_chain = remote_log_chain.then(() => fetch(new URL('/exit?' + code, base_url), { cache: 'no-store' })).catch(() => {});
        }
        throw new Error('proc_exit ' + code);
    },
};

const wgpu_type_adapter = 1;
const wgpu_type_device = 2;
const wgpu_type_queue = 3;
const wgpu_type_canvas_context = 4;
const wgpu_type_buffer = 5;
const wgpu_type_texture = 6;
const wgpu_type_texture_view = 7;
const wgpu_type_sampler = 8;
const wgpu_type_bind_group_layout = 9;
const wgpu_type_bind_group = 10;
const wgpu_type_pipeline_layout = 11;
const wgpu_type_shader_module = 12;
const wgpu_type_render_pipeline = 13;
const wgpu_type_compute_pipeline = 14;
const wgpu_type_command_encoder = 15;
const wgpu_type_command_buffer = 16;
const wgpu_type_render_pass_encoder = 17;
const wgpu_type_compute_pass_encoder = 18;
const wgpu_type_query_set = 19;

const wgpu_objects = new Map();
let wgpu_next_id = 1;

function wgpu_add(type, value) {
    const id = wgpu_next_id++;
    wgpu_objects.set(id, { t: type, v: value });
    return id;
}

function wgpu_get(id) {
    return wgpu_objects.get(id).v;
}

const wgpu_power_preference = [undefined, 'low-power', 'high-performance'];
const wgpu_format = [undefined, 'bgra8unorm', 'r8unorm', 'rgba16float', 'r32float'];
const wgpu_canvas_color_space = [undefined, 'srgb', 'display-p3'];
const wgpu_address_mode = [undefined, 'clamp-to-edge', 'repeat', 'mirror-repeat'];
const wgpu_filter = [undefined, 'nearest', 'linear'];
const wgpu_buffer_binding_type = [undefined, 'uniform', 'storage', 'read-only-storage'];
const wgpu_sample_type = [undefined, 'float', 'unfilterable-float'];
const wgpu_view_dimension = [undefined, '2d'];
const wgpu_storage_access = [undefined, 'write-only'];
const wgpu_sampler_binding_type = [undefined, 'filtering'];
const wgpu_blend_operation = [undefined, 'add'];
const wgpu_blend_factor = [undefined, 'zero', 'one', 'src-alpha', 'one-minus-src-alpha'];
const wgpu_load_op = [undefined, 'load', 'clear'];
const wgpu_store_op = [undefined, 'store', 'discard'];
const wgpu_query_type = [undefined, 'timestamp'];

function wgpu_read_timestamp_writes(ptr) {
    const query_set = ru32(ptr);
    if (query_set === 0) {
        return undefined;
    }
    return {
        querySet: wgpu_get(query_set),
        beginningOfPassWriteIndex: ri32(ptr + 4),
        endOfPassWriteIndex: ri32(ptr + 8),
    };
}

const wgpu_prime_worker_count = navigator.hardwareConcurrency || 4;
const wgpu_prime_timeout_ms = 20000;
let wgpu_prime_batch = null;

const wgpu_prime_source = `
self.onmessage = async (e) => {
    const d = e.data;
    const claims = new Int32Array(d.claims);
    try {
        const adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
        const device = await adapter.requestDevice({ requiredFeatures: d.features, requiredLimits: d.limits });
        const layouts = new Map();
        const layout = (groups) => {
            const key = JSON.stringify(groups);
            if (!layouts.has(key)) {
                layouts.set(key, device.createPipelineLayout({ bindGroupLayouts: groups.map((entries) => device.createBindGroupLayout({ entries: entries })) }));
            }
            return layouts.get(key);
        };
        for (let i = 0; i < d.jobs.length; i++) {
            if (Atomics.compareExchange(claims, i, 0, 1) !== 0) {
                continue;
            }
            const job = d.jobs[i];
            const vertex = device.createShaderModule({ code: job.code });
            if (job.compute) {
                await device.createComputePipelineAsync({ layout: layout(job.layouts), compute: { module: vertex, entryPoint: job.compute } });
            } else {
                const fragment = job.fragment.code === job.code ? vertex : device.createShaderModule({ code: job.fragment.code });
                await device.createRenderPipelineAsync({ layout: layout(job.layouts), vertex: { module: vertex, entryPoint: job.vertex }, fragment: { module: fragment, entryPoint: job.fragment.entryPoint, targets: job.fragment.targets } });
            }
            self.postMessage(i);
        }
        device.destroy();
    } catch (error) {
    }
    self.postMessage(-1);
};
`;

function wgpu_prime_cost(job) {
    if (job.compute) {
        return 3;
    }
    return job.layouts.some((group) => group.some((entry) => entry.texture || entry.storageTexture)) ? 2 : 1;
}

function wgpu_prime_flush() {
    const batch = wgpu_prime_batch;
    wgpu_prime_batch = null;
    const device = batch.device;
    const jobs = batch.jobs;
    if (!wgpu_objects.has(device) || jobs.length < 2) {
        jobs.forEach((job) => job.run());
        return;
    }
    const gpu_device = wgpu_get(device);
    const order = jobs.map((job, i) => i).sort((a, b) => wgpu_prime_cost(jobs[b].data) - wgpu_prime_cost(jobs[a].data) || jobs[b].data.code.length - jobs[a].data.code.length || a - b);
    const count = Math.min(wgpu_prime_worker_count, order.length);
    const url = URL.createObjectURL(new Blob([wgpu_prime_source], { type: 'text/javascript' }));
    const claims = new SharedArrayBuffer(order.length * 4);
    const message = {
        features: Array.from(gpu_device.features),
        limits: { maxComputeWorkgroupStorageSize: gpu_device.limits.maxComputeWorkgroupStorageSize },
        jobs: order.map((i) => jobs[i].data),
        claims: claims,
    };
    const released = new Set();
    const release = (k) => {
        if (!released.has(k)) {
            released.add(k);
            jobs[order[k]].run();
        }
    };
    const workers = [];
    let pending = count;
    let timer = 0;
    const finish = () => {
        clearTimeout(timer);
        for (let k = 0; k < order.length; k++) {
            release(k);
        }
        workers.forEach((worker) => worker.terminate());
        URL.revokeObjectURL(url);
    };
    timer = setTimeout(finish, wgpu_prime_timeout_ms);
    for (let w = 0; w < count; w++) {
        const worker = new Worker(url);
        let ended = false;
        const end = () => {
            if (!ended) {
                ended = true;
                if (--pending === 0) {
                    finish();
                }
            }
        };
        worker.onmessage = (e) => {
            if (e.data >= 0) {
                release(e.data);
            } else {
                end();
            }
        };
        worker.onerror = end;
        worker.postMessage(message);
        workers.push(worker);
    }
}

function wgpu_prime(device, job) {
    if (wgpu_prime_batch === null) {
        wgpu_prime_batch = { device: device, jobs: [] };
        setTimeout(wgpu_prime_flush, 0);
    }
    wgpu_prime_batch.jobs.push(job);
}

function wgpu_layout_groups(layout) {
    return wgpu_objects.get(layout).layouts.map((group) => wgpu_objects.get(group).entries);
}

function wgpu_pipeline_async(type, device, data, make, callback, user) {
    const cb = wasm_table.get(callback);
    const id = wgpu_add(type, null);
    const live = () => wgpu_objects.has(id) && wgpu_objects.has(device);
    const job = { data: data, run: null };
    job.run = () => {
        if (!live()) {
            return;
        }
        make().then((pipeline) => {
            if (live()) {
                wgpu_objects.get(id).v = pipeline;
                cb(id, user, 0);
            }
        }, (error) => {
            if (live()) {
                with_cstr(String(error && error.message ? error.message : error), (message) => cb(id, user, message));
            }
        });
    };
    wgpu_prime(device, job);
    return id;
}

const wgpu = {
    wgpu_object_type(object) {
        const entry = wgpu_objects.get(object);
        return entry ? entry.t : 0;
    },
    wgpu_object_destroy(object) {
        const entry = wgpu_objects.get(object);
        if (entry === undefined) {
            return;
        }
        if (entry.t === wgpu_type_buffer || entry.t === wgpu_type_texture || entry.t === wgpu_type_query_set || entry.t === wgpu_type_device) {
            entry.v.destroy();
        }
        wgpu_objects.delete(object);
    },
    navigator_gpu_request_adapter_async(options, callback, user) {
        const cb = wasm_table.get(callback);
        const power = ru32(options);
        if (navigator.gpu === undefined) {
            console.error('webgpu is not supported in this browser');
            cb(0, user);
            return;
        }
        navigator.gpu.requestAdapter(power ? { powerPreference: wgpu_power_preference[power] } : {}).then(
            (adapter) => cb(adapter ? wgpu_add(wgpu_type_adapter, adapter) : 0, user),
            () => cb(0, user));
    },
    wgpu_adapter_or_device_get_features(object) {
        const features = wgpu_get(object).features;
        return (features.has('timestamp-query') ? 1 : 0) | (features.has('shader-f16') ? 2 : 0);
    },
    wgpu_adapter_or_device_get_workgroup_storage_limit(object) {
        return wgpu_get(object).limits.maxComputeWorkgroupStorageSize;
    },
    wgpu_adapter_request_device_async(adapter, descriptor, callback, user) {
        const cb = wasm_table.get(callback);
        const features = [];
        if (ru32(descriptor) & 1) {
            features.push('timestamp-query');
        }
        if (ru32(descriptor) & 2) {
            features.push('shader-f16');
        }
        const limits = {};
        const workgroup_storage = ru32(descriptor + 4);
        if (workgroup_storage) {
            limits.maxComputeWorkgroupStorageSize = workgroup_storage;
        }
        Promise.all([wgpu_get(adapter).requestDevice({ requiredFeatures: features, requiredLimits: limits }), pool_ready]).then(
            ([device]) => cb(wgpu_add(wgpu_type_device, device), user),
            () => cb(0, user));
    },
    wgpu_device_get_queue(device) {
        return wgpu_add(wgpu_type_queue, wgpu_get(device).queue);
    },
    wgpu_device_set_uncapturederror_callback(device, callback, user) {
        const cb = wasm_table.get(callback);
        wgpu_get(device).onuncapturederror = (e) => {
            const type = e.error instanceof GPUValidationError ? 1 : (e.error instanceof GPUOutOfMemoryError ? 2 : 3);
            with_cstr(e.error.message, (msg) => cb(device, type, msg, user));
        };
    },
    wgpu_device_set_lost_callback(device, callback, user) {
        const cb = wasm_table.get(callback);
        wgpu_get(device).lost.then((info) => {
            if (wgpu_objects.has(device)) {
                with_cstr(info.message, (msg) => cb(device, info.reason === 'destroyed' ? 1 : 0, msg, user));
            }
        });
    },
    wgpu_canvas_get_webgpu_context(canvas_id) {
        const context = document.getElementById(cstr(canvas_id)).getContext('webgpu');
        return context ? wgpu_add(wgpu_type_canvas_context, context) : 0;
    },
    wgpu_canvas_context_configure(context, config) {
        try {
            wgpu_get(context).configure({
                device: wgpu_get(ru32(config)),
                format: wgpu_format[ru32(config + 4)],
                colorSpace: wgpu_canvas_color_space[ru32(config + 8)],
                alphaMode: 'premultiplied',
                usage: ru32(config + 12),
                toneMapping: { mode: ru32(config + 16) ? 'extended' : 'standard' },
            });
        } catch (e) {
            return 0;
        }
        return 1;
    },
    wgpu_canvas_context_get_current_texture_view(context) {
        const entry = wgpu_objects.get(context);
        if (entry.view) {
            wgpu_objects.delete(entry.view);
        }
        const texture = entry.v.getCurrentTexture();
        entry.view = wgpu_add(wgpu_type_texture_view, texture.createView());
        wgpu_objects.get(entry.view).f = wgpu_format.indexOf(texture.format);
        wgpu_objects.get(entry.view).u = texture.usage;
        return entry.view;
    },
    wgpu_device_create_shader_module(device, descriptor) {
        const code = cstr(ru32(descriptor));
        const id = wgpu_add(wgpu_type_shader_module, wgpu_get(device).createShaderModule({ code: code }));
        wgpu_objects.get(id).code = code;
        return id;
    },
    wgpu_device_create_sampler(device, descriptor) {
        return wgpu_add(wgpu_type_sampler, wgpu_get(device).createSampler({
            addressModeU: wgpu_address_mode[ru32(descriptor)],
            addressModeV: wgpu_address_mode[ru32(descriptor + 4)],
            addressModeW: wgpu_address_mode[ru32(descriptor + 8)],
            minFilter: wgpu_filter[ru32(descriptor + 12)],
            magFilter: wgpu_filter[ru32(descriptor + 16)],
            mipmapFilter: wgpu_filter[ru32(descriptor + 20)],
        }));
    },
    wgpu_device_create_buffer(device, descriptor) {
        return wgpu_add(wgpu_type_buffer, wgpu_get(device).createBuffer({
            size: ru32(descriptor),
            usage: ru32(descriptor + 4),
        }));
    },
    wgpu_device_create_texture(device, descriptor) {
        return wgpu_add(wgpu_type_texture, wgpu_get(device).createTexture({
            size: { width: ru32(descriptor), height: ru32(descriptor + 4) },
            format: wgpu_format[ru32(descriptor + 8)],
            usage: ru32(descriptor + 12),
            mipLevelCount: Math.max(ru32(descriptor + 16), 1),
        }));
    },
    wgpu_device_create_bind_group_layout(device, entries, entry_count) {
        const out = [];
        for (let i = 0; i < entry_count; i++) {
            const e = entries + i * 24;
            const entry = {
                binding: ru32(e),
                visibility: ru32(e + 4),
            };
            const type = ru32(e + 8);
            const a = ru32(e + 12);
            const b = ru32(e + 16);
            const c = ru32(e + 20);
            if (type === 1) {
                entry.buffer = { type: wgpu_buffer_binding_type[a], hasDynamicOffset: !!b };
            } else if (type === 2) {
                entry.sampler = { type: wgpu_sampler_binding_type[a] };
            } else if (type === 3) {
                entry.texture = { sampleType: wgpu_sample_type[a], viewDimension: wgpu_view_dimension[b] };
            } else {
                entry.storageTexture = { access: wgpu_storage_access[a], format: wgpu_format[b], viewDimension: wgpu_view_dimension[c] };
            }
            out.push(entry);
        }
        const id = wgpu_add(wgpu_type_bind_group_layout, wgpu_get(device).createBindGroupLayout({ entries: out }));
        wgpu_objects.get(id).entries = out;
        return id;
    },
    wgpu_device_create_bind_group(device, layout, entries, entry_count) {
        const out = [];
        for (let i = 0; i < entry_count; i++) {
            const e = entries + i * 16;
            const res = wgpu_objects.get(ru32(e + 4));
            let resource;
            if (res.t === wgpu_type_buffer) {
                resource = { buffer: res.v, offset: ru32(e + 8) };
                const size = ru32(e + 12);
                if (size) {
                    resource.size = size;
                }
            } else {
                resource = res.v;
            }
            out.push({ binding: ru32(e), resource: resource });
        }
        return wgpu_add(wgpu_type_bind_group, wgpu_get(device).createBindGroup({ layout: wgpu_get(layout), entries: out }));
    },
    wgpu_device_create_pipeline_layout(device, layouts, layout_count) {
        const ids = [];
        for (let i = 0; i < layout_count; i++) {
            ids.push(ru32(layouts + i * 4));
        }
        const id = wgpu_add(wgpu_type_pipeline_layout, wgpu_get(device).createPipelineLayout({ bindGroupLayouts: ids.map(wgpu_get) }));
        wgpu_objects.get(id).layouts = ids;
        return id;
    },
    wgpu_device_create_render_pipeline_async(device, descriptor, callback, user) {
        const num_targets = ru32(descriptor + 20);
        const targets_ptr = ru32(descriptor + 24);
        const targets = [];
        for (let i = 0; i < num_targets; i++) {
            const t = targets_ptr + i * 28;
            const target = { format: wgpu_format[ru32(t)] };
            const color_operation = ru32(t + 4);
            if (color_operation !== 0) {
                target.blend = {
                    color: { operation: wgpu_blend_operation[color_operation], srcFactor: wgpu_blend_factor[ru32(t + 8)], dstFactor: wgpu_blend_factor[ru32(t + 12)] },
                    alpha: { operation: wgpu_blend_operation[ru32(t + 16)], srcFactor: wgpu_blend_factor[ru32(t + 20)], dstFactor: wgpu_blend_factor[ru32(t + 24)] },
                };
            }
            targets.push(target);
        }
        const layout = ru32(descriptor);
        const vertex = wgpu_objects.get(ru32(descriptor + 4));
        const fragment = wgpu_objects.get(ru32(descriptor + 12));
        const desc = {
            layout: wgpu_get(layout),
            vertex: { module: vertex.v, entryPoint: cstr(ru32(descriptor + 8)) },
            fragment: { module: fragment.v, entryPoint: cstr(ru32(descriptor + 16)), targets: targets },
        };
        const data = {
            code: vertex.code,
            vertex: desc.vertex.entryPoint,
            fragment: { code: fragment.code, entryPoint: desc.fragment.entryPoint, targets: targets },
            compute: null,
            layouts: wgpu_layout_groups(layout),
        };
        const gpu_device = wgpu_get(device);
        return wgpu_pipeline_async(wgpu_type_render_pipeline, device, data, () => gpu_device.createRenderPipelineAsync(desc), callback, user);
    },
    wgpu_device_create_compute_pipeline_async(device, module, entry_point, layout, callback, user) {
        const entry = wgpu_objects.get(module);
        const desc = { layout: wgpu_get(layout), compute: { module: entry.v, entryPoint: cstr(entry_point) } };
        const data = { code: entry.code, vertex: null, fragment: null, compute: desc.compute.entryPoint, layouts: wgpu_layout_groups(layout) };
        const gpu_device = wgpu_get(device);
        return wgpu_pipeline_async(wgpu_type_compute_pipeline, device, data, () => gpu_device.createComputePipelineAsync(desc), callback, user);
    },
    wgpu_device_create_query_set(device, descriptor) {
        return wgpu_add(wgpu_type_query_set, wgpu_get(device).createQuerySet({
            type: wgpu_query_type[ru32(descriptor)],
            count: ru32(descriptor + 4),
        }));
    },
    wgpu_device_create_command_encoder_simple(device) {
        return wgpu_add(wgpu_type_command_encoder, wgpu_get(device).createCommandEncoder());
    },
    wgpu_texture_create_view_simple(texture) {
        const view = wgpu_add(wgpu_type_texture_view, wgpu_get(texture).createView());
        wgpu_objects.get(view).f = wgpu_format.indexOf(wgpu_get(texture).format);
        wgpu_objects.get(view).u = wgpu_get(texture).usage;
        return view;
    },
    wgpu_texture_view_get_format(view) {
        const f = wgpu_objects.get(view).f;
        return f > 0 ? f : 0;
    },
    wgpu_texture_view_get_usage(view) {
        return wgpu_objects.get(view).u || 0;
    },
    wgpu_texture_get_usage(texture) {
        return wgpu_get(texture).usage;
    },
    wgpu_texture_create_view_mip(texture, level) {
        return wgpu_add(wgpu_type_texture_view, wgpu_get(texture).createView({ baseMipLevel: level, mipLevelCount: 1 }));
    },
    wgpu_queue_write_buffer(queue, buffer, buffer_offset, data, size) {
        wgpu_get(queue).writeBuffer(wgpu_get(buffer), buffer_offset, wasm_memory.buffer, data, size);
    },
    wgpu_queue_write_texture(queue, destination, data, bytes_per_block_row, block_rows_per_image, write_width, write_height, write_depth) {
        wgpu_get(queue).writeTexture(
            { texture: wgpu_get(ru32(destination)), origin: { x: ri32(destination + 4), y: ri32(destination + 8) } },
            wasm_memory.buffer,
            { offset: data, bytesPerRow: bytes_per_block_row, rowsPerImage: block_rows_per_image },
            { width: write_width, height: write_height, depthOrArrayLayers: write_depth });
    },
    wgpu_queue_set_on_submitted_work_done_callback(queue, callback, user) {
        const cb = wasm_table.get(callback);
        const done = () => {
            if (wgpu_objects.has(queue)) {
                cb(queue, user);
            }
        };
        wgpu_get(queue).onSubmittedWorkDone().then(done, done);
    },
    wgpu_queue_submit_one_and_destroy(queue, command_buffer) {
        wgpu_get(queue).submit([wgpu_get(command_buffer)]);
        wgpu_objects.delete(command_buffer);
    },
    wgpu_buffer_map_async(buffer, callback, user, mode, offset, size) {
        const cb = wasm_table.get(callback);
        const done = () => {
            if (wgpu_objects.has(buffer)) {
                cb(buffer, user, mode, offset, size);
            }
        };
        wgpu_get(buffer).mapAsync(mode, offset, size).then(done, done);
    },
    wgpu_command_encoder_copy_texture_to_buffer(encoder, source, destination, bytes_per_row, width, height) {
        wgpu_get(encoder).copyTextureToBuffer(
            { texture: wgpu_get(source) },
            { buffer: wgpu_get(destination), bytesPerRow: bytes_per_row },
            { width: width, height: height });
    },
    wgpu_buffer_get_mapped_range(buffer, offset, size) {
        const entry = wgpu_objects.get(buffer);
        try {
            entry.mapped = entry.v.getMappedRange(offset, size);
            return 0;
        } catch (e) {
            entry.mapped = null;
            return -1;
        }
    },
    wgpu_buffer_read_mapped_range(buffer, start_offset, sub_offset, dst, size) {
        mu8().set(new Uint8Array(wgpu_objects.get(buffer).mapped, sub_offset, size), dst);
    },
    wgpu_buffer_unmap(buffer) {
        const entry = wgpu_objects.get(buffer);
        entry.mapped = null;
        entry.v.unmap();
    },
    wgpu_command_encoder_begin_render_pass(encoder, descriptor) {
        const num_attachments = ru32(descriptor);
        const attachments_ptr = ru32(descriptor + 4);
        const attachments = [];
        for (let i = 0; i < num_attachments; i++) {
            const a = attachments_ptr + i * 28;
            attachments.push({
                view: wgpu_get(ru32(a)),
                loadOp: wgpu_load_op[ru32(a + 4)],
                storeOp: wgpu_store_op[ru32(a + 8)],
                clearValue: { r: rf32(a + 12), g: rf32(a + 16), b: rf32(a + 20), a: rf32(a + 24) },
            });
        }
        const desc = { colorAttachments: attachments };
        const timestamp_writes = wgpu_read_timestamp_writes(descriptor + 8);
        if (timestamp_writes) {
            desc.timestampWrites = timestamp_writes;
        }
        return wgpu_add(wgpu_type_render_pass_encoder, wgpu_get(encoder).beginRenderPass(desc));
    },
    wgpu_command_encoder_begin_compute_pass(encoder, descriptor) {
        const desc = {};
        const timestamp_writes = wgpu_read_timestamp_writes(descriptor);
        if (timestamp_writes) {
            desc.timestampWrites = timestamp_writes;
        }
        return wgpu_add(wgpu_type_compute_pass_encoder, wgpu_get(encoder).beginComputePass(desc));
    },
    wgpu_command_encoder_copy_buffer_to_buffer(encoder, source, source_offset, destination, destination_offset, size) {
        wgpu_get(encoder).copyBufferToBuffer(wgpu_get(source), source_offset, wgpu_get(destination), destination_offset, size);
    },
    wgpu_command_encoder_resolve_query_set(encoder, query_set, first_query, query_count, destination, destination_offset) {
        wgpu_get(encoder).resolveQuerySet(wgpu_get(query_set), first_query, query_count, wgpu_get(destination), destination_offset);
    },
    wgpu_command_encoder_finish(encoder) {
        const command_buffer = wgpu_get(encoder).finish();
        wgpu_objects.delete(encoder);
        return wgpu_add(wgpu_type_command_buffer, command_buffer);
    },
    wgpu_render_pass_encoder_set_pipeline(pass, pipeline) {
        wgpu_get(pass).setPipeline(wgpu_get(pipeline));
    },
    wgpu_render_pass_encoder_set_bind_group(pass, index, group, dynamic_offsets, dynamic_offset_count) {
        if (dynamic_offset_count) {
            const offsets = new Uint32Array(dynamic_offset_count);
            offsets.set(new Uint32Array(wasm_memory.buffer, dynamic_offsets, dynamic_offset_count));
            wgpu_get(pass).setBindGroup(index, wgpu_get(group), offsets);
        } else {
            wgpu_get(pass).setBindGroup(index, wgpu_get(group));
        }
    },
    wgpu_render_pass_encoder_set_scissor_rect(pass, x, y, width, height) {
        wgpu_get(pass).setScissorRect(x, y, width, height);
    },
    wgpu_render_pass_encoder_draw(pass, vertex_count, instance_count, first_vertex, first_instance) {
        wgpu_get(pass).draw(vertex_count, instance_count, first_vertex, first_instance);
    },
    wgpu_render_pass_encoder_end(pass) {
        wgpu_get(pass).end();
        wgpu_objects.delete(pass);
    },
    wgpu_compute_pass_encoder_set_pipeline(pass, pipeline) {
        wgpu_get(pass).setPipeline(wgpu_get(pipeline));
    },
    wgpu_compute_pass_encoder_set_bind_group(pass, index, group, dynamic_offsets, dynamic_offset_count) {
        if (dynamic_offset_count) {
            const offsets = new Uint32Array(dynamic_offset_count);
            offsets.set(new Uint32Array(wasm_memory.buffer, dynamic_offsets, dynamic_offset_count));
            wgpu_get(pass).setBindGroup(index, wgpu_get(group), offsets);
        } else {
            wgpu_get(pass).setBindGroup(index, wgpu_get(group));
        }
    },
    wgpu_compute_pass_encoder_dispatch_workgroups(pass, x, y, z) {
        wgpu_get(pass).dispatchWorkgroups(x, y, z);
    },
    wgpu_compute_pass_encoder_dispatch_workgroups_indirect(pass, indirect_buffer, indirect_offset) {
        wgpu_get(pass).dispatchWorkgroupsIndirect(wgpu_get(indirect_buffer), indirect_offset);
    },
    wgpu_compute_pass_encoder_end(pass) {
        wgpu_get(pass).end();
        wgpu_objects.delete(pass);
    },
};

let app_ptr = 0;
let text_input = null;
let text_input_active = false;
let text_input_canvas = 0;

const text_command = {
    move_left: 0, move_right: 1, move_up: 2, move_down: 3,
    move_word_left: 4, move_word_right: 5,
    move_line_start: 6, move_line_end: 7,
    move_doc_start: 8, move_doc_end: 9,
    delete_backward: 10, delete_forward: 11,
    delete_word_backward: 12, delete_word_forward: 13,
    insert_newline: 14, insert_tab: 15,
    select_all: 16, copy: 17, cut: 18,
    delete_line: 19, delete_line_start: 20, delete_line_end: 21,
    move_page_up: 22, move_page_down: 23,
    scroll_doc_start: 24, scroll_doc_end: 25,
    scroll_page_up: 26, scroll_page_down: 27,
};

const text_input_is_mac = navigator.platform.toLowerCase().includes('mac') || navigator.userAgent.includes('Mac');

const text_input_sentinel_around_caret = '  ';

function text_input_reset(ta) {
    if (ta.value !== text_input_sentinel_around_caret) {
        ta.value = text_input_sentinel_around_caret;
    }
    ta.selectionStart = 1;
    ta.selectionEnd = 1;
}

function send_text_command(command, select) {
    if (app_ptr === 0) {
        return;
    }
    wasm_exports.app_wasm_handle_text_command(app_ptr, text_input_canvas, command, select ? 1 : 0);
}

function send_text(str) {
    if (app_ptr === 0 || str.length === 0) {
        return;
    }
    const bytes = text_encoder.encode(str);
    const ptr = wasm_exports.malloc(bytes.length + 1);
    mu8().set(bytes, ptr);
    mu8()[ptr + bytes.length] = 0;
    wasm_exports.app_wasm_handle_text(app_ptr, text_input_canvas, ptr, bytes.length);
    wasm_exports.free(ptr);
}

function send_marked_text(str) {
    if (app_ptr === 0) {
        return;
    }
    const bytes = text_encoder.encode(str);
    const ptr = wasm_exports.malloc(bytes.length + 1);
    mu8().set(bytes, ptr);
    mu8()[ptr + bytes.length] = 0;
    wasm_exports.app_wasm_handle_marked_text(app_ptr, text_input_canvas, ptr, bytes.length, bytes.length);
    wasm_exports.free(ptr);
}

function text_input_create() {
    const ta = document.createElement('textarea');
    ta.setAttribute('autocorrect', 'off');
    ta.setAttribute('autocapitalize', 'off');
    ta.setAttribute('autocomplete', 'off');
    ta.setAttribute('spellcheck', 'false');
    ta.style.position = 'fixed';
    ta.style.left = '0px';
    ta.style.top = '0px';
    ta.style.width = '4px';
    ta.style.height = '4px';
    ta.style.padding = '0px';
    ta.style.border = 'none';
    ta.style.outline = 'none';
    ta.style.opacity = '0';
    ta.style.background = 'transparent';
    ta.style.color = 'transparent';
    ta.style.caretColor = 'transparent';
    ta.style.resize = 'none';
    ta.style.pointerEvents = 'none';
    ta.style.overflow = 'hidden';
    ta.style.webkitUserSelect = 'text';
    ta.style.userSelect = 'text';
    document.body.appendChild(ta);
    text_input_reset(ta);

    ta.addEventListener('focus', function() {
        text_input_reset(ta);
    });

    ta.addEventListener('keyup', function(e) {
        send_key(text_input_canvas, e, false);
    });

    ta.addEventListener('keydown', function(e) {
        if (e.isComposing || e.keyCode === 229) {
            return;
        }
        send_key(text_input_canvas, e, true);
        const select = e.shiftKey;
        const word = text_input_is_mac ? e.altKey : e.ctrlKey;
        const line = text_input_is_mac && e.metaKey;
        const short = text_input_is_mac ? e.metaKey : e.ctrlKey;
        let handled = true;
        switch (e.key) {
        case 'ArrowLeft': send_text_command(line ? text_command.move_line_start : (word ? text_command.move_word_left : text_command.move_left), select); break;
        case 'ArrowRight': send_text_command(line ? text_command.move_line_end : (word ? text_command.move_word_right : text_command.move_right), select); break;
        case 'ArrowUp': send_text_command(line ? text_command.move_doc_start : text_command.move_up, select); break;
        case 'ArrowDown': send_text_command(line ? text_command.move_doc_end : text_command.move_down, select); break;
        case 'Home': send_text_command(text_input_is_mac ? text_command.scroll_doc_start : (e.ctrlKey ? text_command.move_doc_start : text_command.move_line_start), text_input_is_mac ? false : select); break;
        case 'End': send_text_command(text_input_is_mac ? text_command.scroll_doc_end : (e.ctrlKey ? text_command.move_doc_end : text_command.move_line_end), text_input_is_mac ? false : select); break;
        case 'PageUp': send_text_command(text_input_is_mac ? text_command.scroll_page_up : text_command.move_page_up, text_input_is_mac ? false : select); break;
        case 'PageDown': send_text_command(text_input_is_mac ? text_command.scroll_page_down : text_command.move_page_down, text_input_is_mac ? false : select); break;
        case 'Backspace': send_text_command(line ? text_command.delete_line_start : (word ? text_command.delete_word_backward : text_command.delete_backward), false); break;
        case 'Delete': send_text_command(word ? text_command.delete_word_forward : text_command.delete_forward, false); break;
        case 'Enter': send_text_command(text_command.insert_newline, false); break;
        case 'Tab': send_text_command(text_command.insert_tab, false); break;
        default: {
            const key = e.key.toLowerCase();
            const extra = e.altKey || (text_input_is_mac ? e.ctrlKey : e.metaKey);
            if (short && !extra && e.shiftKey && key === 'k') {
                send_text_command(text_command.delete_line, false);
            } else if (short && !extra && !e.shiftKey && key === 'a') {
                send_text_command(text_command.select_all, false);
            } else if (short && !extra && !e.shiftKey && key === 'c') {
                send_text_command(text_command.copy, false);
            } else if (short && !extra && !e.shiftKey && key === 'x') {
                send_text_command(text_command.cut, false);
            } else if (text_input_is_mac && e.ctrlKey && !e.metaKey && !e.altKey && key === 'k') {
                send_text_command(text_command.delete_line_end, false);
            } else {
                handled = false;
            }
        } break;
        }
        if (handled) {
            e.preventDefault();
        }
    });

    ta.addEventListener('beforeinput', function(e) {
        switch (e.inputType) {
        case 'insertText': {
            if (e.data) {
                send_text(e.data);
            }
            e.preventDefault();
        } break;
        case 'insertLineBreak':
        case 'insertParagraph': {
            send_text_command(text_command.insert_newline, false);
            e.preventDefault();
        } break;
        case 'deleteContentBackward': {
            send_text_command(text_command.delete_backward, false);
            e.preventDefault();
        } break;
        case 'deleteContentForward': {
            send_text_command(text_command.delete_forward, false);
            e.preventDefault();
        } break;
        case 'deleteWordBackward': {
            send_text_command(text_command.delete_word_backward, false);
            e.preventDefault();
        } break;
        case 'deleteWordForward': {
            send_text_command(text_command.delete_word_forward, false);
            e.preventDefault();
        } break;
        case 'insertFromPaste':
        case 'insertFromDrop': {
            const data = e.data || (e.dataTransfer ? e.dataTransfer.getData('text/plain') : '');
            if (data) {
                send_text(data);
            }
            e.preventDefault();
        } break;
        default: break;
        }
    });

    ta.addEventListener('paste', function(e) {
        const data = e.clipboardData ? e.clipboardData.getData('text/plain') : '';
        if (data) {
            send_text(data);
        }
        e.preventDefault();
    });

    ta.addEventListener('compositionupdate', function(e) {
        send_marked_text(e.data || '');
    });

    ta.addEventListener('compositionend', function(e) {
        send_marked_text('');
        if (e.data) {
            send_text(e.data);
        }
        text_input_reset(ta);
    });

    ta.addEventListener('blur', function() {
        setTimeout(function() {
            if (app_ptr !== 0 && text_input_active && document.hasFocus() && document.activeElement !== ta) {
                wasm_exports.app_wasm_handle_text_input_dismissed(app_ptr, text_input_canvas);
            }
        }, 0);
    });

    return ta;
}

const pointer_event_type = { hover_began: 0, hover_moved: 1, hover_ended: 2, touch_began: 3, touch_moved: 4, touch_ended: 5, canceled: 6 };
const pointer_source = { direct: 0, indirect: 1, pencil: 2 };
const pointer_button = { primary: 0, secondary: 1, middle: 2 };
const modifier = { shift: 1, control: 2, alt: 4, command: 8 };
const window_style = { bordered: 0, borderless: 1, transparent: 2 };
const cursor_type = { default: 0, text: 1 };

const key_code_hid = (function() {
    const map = {
        Enter: 0x28, Escape: 0x29, Backspace: 0x2a, Tab: 0x2b, Space: 0x2c, Minus: 0x2d, Equal: 0x2e,
        BracketLeft: 0x2f, BracketRight: 0x30, Backslash: 0x31, Semicolon: 0x33, Quote: 0x34, Backquote: 0x35,
        Comma: 0x36, Period: 0x37, Slash: 0x38, CapsLock: 0x39,
        Home: 0x4a, PageUp: 0x4b, Delete: 0x4c, End: 0x4d, PageDown: 0x4e,
        ArrowRight: 0x4f, ArrowLeft: 0x50, ArrowDown: 0x51, ArrowUp: 0x52,
        NumpadDivide: 0x54, NumpadMultiply: 0x55, NumpadSubtract: 0x56, NumpadAdd: 0x57, NumpadEnter: 0x58, NumpadDecimal: 0x63, Numpad0: 0x62,
        ControlLeft: 0xe0, ShiftLeft: 0xe1, AltLeft: 0xe2, MetaLeft: 0xe3, ControlRight: 0xe4, ShiftRight: 0xe5, AltRight: 0xe6, MetaRight: 0xe7,
    };
    for (let i = 0; i < 26; i++) {
        map['Key' + String.fromCharCode(65 + i)] = 0x04 + i;
    }
    for (let i = 1; i <= 9; i++) {
        map['Digit' + i] = 0x1e + i - 1;
        map['Numpad' + i] = 0x59 + i - 1;
    }
    map.Digit0 = 0x27;
    for (let i = 1; i <= 12; i++) {
        map['F' + i] = 0x3a + i - 1;
    }
    return map;
})();

function pointer_event_source(e) {
    if (e.pointerType === 'pen') {
        return pointer_source.pencil;
    }
    if (e.pointerType === 'touch') {
        return pointer_source.direct;
    }
    return pointer_source.indirect;
}

function pointer_event_id(e) {
    return e.pointerId + 1;
}

function pointer_event_button(e) {
    if (e.button === 2) {
        return pointer_button.secondary;
    }
    if (e.button === 1) {
        return pointer_button.middle;
    }
    return pointer_button.primary;
}

function event_modifiers(e) {
    return (e.shiftKey ? modifier.shift : 0) | (e.ctrlKey ? modifier.control : 0) | (e.altKey ? modifier.alt : 0) | (e.metaKey ? modifier.command : 0);
}

function event_timestamp(e) {
    return e.timeStamp / 1000;
}

function send_key(canvas_id, e, down) {
    if (app_ptr === 0 || canvas_id === 0) {
        return;
    }
    const key = key_code_hid[e.code] || 0;
    wasm_exports.app_wasm_handle_key(app_ptr, canvas_id, key, event_modifiers(e), down ? 1 : 0, e.repeat ? 1 : 0);
}

const canvases = new Map();
let canvas_next_id = 1;

function canvas_get(id) {
    return canvases.get(id);
}

function canvas_push_size(entry) {
    if (app_ptr === 0 || !wasm_exports) {
        return;
    }
    const dpr = window.devicePixelRatio;
    const px_w = entry.px_exact ? entry.px_w : Math.round(entry.pt_w * dpr);
    const px_h = entry.px_exact ? entry.px_h : Math.round(entry.pt_h * dpr);
    wasm_exports.app_wasm_handle_size(app_ptr, entry.id, entry.pt_w, entry.pt_h, px_w, px_h, dpr);
}

function canvas_size_from_box(entry, resize_entry) {
    const el = entry.el;
    const content = resize_entry.contentBoxSize;
    const box = content ? (Array.isArray(content) ? content[0] : content) : null;
    if (box) {
        entry.pt_w = box.inlineSize;
        entry.pt_h = box.blockSize;
    } else {
        entry.pt_w = el.clientWidth;
        entry.pt_h = el.clientHeight;
    }
    const device = resize_entry.devicePixelContentBoxSize;
    if (device && device.length > 0) {
        entry.px_w = device[0].inlineSize;
        entry.px_h = device[0].blockSize;
        entry.px_exact = true;
    } else {
        entry.px_exact = false;
    }
}

function canvas_observe(entry) {
    const el = entry.el;
    if (typeof ResizeObserver === 'undefined') {
        return;
    }
    const ro = new ResizeObserver(function(entries) {
        for (const resize_entry of entries) {
            if (resize_entry.target === el) {
                canvas_size_from_box(entry, resize_entry);
                canvas_push_size(entry);
            }
        }
    });
    try {
        ro.observe(el, { box: 'device-pixel-content-box' });
    } catch (_) {
        ro.observe(el);
    }
    entry.ro = ro;
}

function canvas_apply_rect(entry) {
    const el = entry.el;
    if (entry.fill) {
        el.style.left = '0px';
        el.style.top = '0px';
        el.style.width = '100%';
        el.style.height = '100%';
        entry.pt_w = window.innerWidth;
        entry.pt_h = window.innerHeight;
    } else {
        const w = Math.min(entry.w, window.innerWidth);
        const h = Math.min(entry.h, window.innerHeight);
        const x = Math.max(0, Math.min(entry.x, window.innerWidth - w));
        const y = Math.max(0, Math.min(entry.y, window.innerHeight - h));
        el.style.left = x + 'px';
        el.style.top = y + 'px';
        el.style.width = w + 'px';
        el.style.height = h + 'px';
        entry.pt_w = w;
        entry.pt_h = h;
    }
    entry.px_exact = false;
    canvas_push_size(entry);
}

function canvas_pixel_ratio_watch() {
    const query = window.matchMedia('(resolution: ' + window.devicePixelRatio + 'dppx)');
    query.addEventListener('change', function() {
        canvases.forEach(function(entry) {
            entry.px_exact = false;
            canvas_push_size(entry);
        });
        canvas_pixel_ratio_watch();
    }, { once: true });
}

function app_js_canvas_create(x, y, w, h, style) {
    const id = canvas_next_id++;
    const el = document.createElement('canvas');
    el.id = 'canvas' + id;
    el.tabIndex = 0;
    el.style.position = 'absolute';
    el.style.outline = 'none';
    if (style === window_style.transparent) {
        el.style.background = 'transparent';
    }
    const entry = { id: id, el: el, x: x, y: y, w: w, h: h, fill: !(w > 0 && h > 0), pt_w: 0, pt_h: 0, px_w: 0, px_h: 0, px_exact: false, ro: null };
    canvas_apply_rect(entry);
    document.body.appendChild(el);
    canvases.set(id, entry);
    canvas_observe(entry);

    const local = function(e) {
        const rect = el.getBoundingClientRect();
        return { x: e.clientX - rect.left, y: e.clientY - rect.top };
    };

    const send_pointer = function(type, e, button) {
        const p = local(e);
        const touching = type === pointer_event_type.touch_began || type === pointer_event_type.touch_moved;
        const pressure = e.pointerType === 'pen' ? e.pressure : (touching ? 1 : 0);
        const radius = e.width * 0.5;
        wasm_exports.app_wasm_handle_pointer(app_ptr, id, type, pointer_event_id(e), pointer_event_source(e), button, event_modifiers(e), pressure, radius, p.x, p.y, event_timestamp(e));
        return p;
    };

    const pointer_down_pos = new Map();
    const pointer_down_button = new Map();

    el.addEventListener('pointerdown', function(e) {
        e.preventDefault();
        el.setPointerCapture(e.pointerId);
        const button = pointer_event_button(e);
        pointer_down_button.set(e.pointerId, button);
        pointer_down_pos.set(e.pointerId, send_pointer(pointer_event_type.touch_began, e, button));
    });

    el.addEventListener('pointermove', function(e) {
        e.preventDefault();
        if (e.buttons) {
            send_pointer(pointer_event_type.touch_moved, e, pointer_down_button.get(e.pointerId) || pointer_button.primary);
        } else {
            send_pointer(pointer_event_type.hover_moved, e, pointer_button.primary);
        }
    });

    el.addEventListener('pointerup', function(e) {
        e.preventDefault();
        el.releasePointerCapture(e.pointerId);
        const button = pointer_down_button.get(e.pointerId);
        pointer_down_button.delete(e.pointerId);
        const p = send_pointer(pointer_event_type.touch_ended, e, button === undefined ? pointer_event_button(e) : button);
        const down = pointer_down_pos.get(e.pointerId);
        pointer_down_pos.delete(e.pointerId);
        const is_tap = down && Math.abs(p.x - down.x) < 10 && Math.abs(p.y - down.y) < 10;
        const wants_focus = (text_input_active && text_input_canvas === id) || (is_tap && wasm_exports.app_wasm_text_input_hit_test(app_ptr, id, p.x, p.y));
        if (wants_focus && text_input) {
            text_input_canvas = id;
            text_input.focus({ preventScroll: true });
        }
    });

    el.addEventListener('mousedown', function(e) {
        e.preventDefault();
    });

    el.addEventListener('touchstart', function(e) {
        e.preventDefault();
    });

    el.addEventListener('pointercancel', function(e) {
        e.preventDefault();
        el.releasePointerCapture(e.pointerId);
        pointer_down_pos.delete(e.pointerId);
        const button = pointer_down_button.get(e.pointerId);
        pointer_down_button.delete(e.pointerId);
        send_pointer(pointer_event_type.canceled, e, button === undefined ? pointer_button.primary : button);
    });

    el.addEventListener('pointerenter', function(e) {
        e.preventDefault();
        send_pointer(pointer_event_type.hover_began, e, pointer_button.primary);
    });

    el.addEventListener('pointerleave', function(e) {
        e.preventDefault();
        send_pointer(pointer_event_type.hover_ended, e, pointer_button.primary);
    });

    el.addEventListener('wheel', function(e) {
        e.preventDefault();
        const to_pt_from_delta = e.deltaMode === 1 ? 13 : (e.deltaMode === 2 ? el.clientHeight : 1);
        const p = local(e);
        wasm_exports.app_wasm_handle_scroll(app_ptr, id, e.deltaX * to_pt_from_delta, e.deltaY * to_pt_from_delta, p.x, p.y, event_timestamp(e));
    });

    el.addEventListener('keydown', function(e) {
        send_key(id, e, true);
    });

    el.addEventListener('keyup', function(e) {
        send_key(id, e, false);
    });

    el.addEventListener('contextmenu', function(e) {
        e.preventDefault();
    });

    el.addEventListener('focus', function() {
        wasm_exports.app_wasm_handle_focus(app_ptr, id, 1);
    });

    el.addEventListener('blur', function() {
        wasm_exports.app_wasm_handle_focus(app_ptr, id, 0);
    });

    return id;
}

function app_js_init(app) {
    app_ptr = app;

    text_input = text_input_create();

    const match_is_dark = window.matchMedia('(prefers-color-scheme: dark)');
    function handle_is_dark(e) {
        if (app_ptr !== 0) {
            wasm_exports.app_wasm_handle_is_dark(app_ptr, e.matches ? 1 : 0);
        }
    }
    match_is_dark.addEventListener('change', handle_is_dark);

    if (window.visualViewport) {
        const vv = window.visualViewport;
        let inset_prev = 0;
        const compute_inset = function() {
            return Math.max(0, window.innerHeight - vv.height - vv.offsetTop);
        };
        const handle_vv = function() {
            if (app_ptr === 0) {
                return;
            }
            const inset = compute_inset();
            wasm_exports.app_wasm_handle_keyboard_inset(app_ptr, inset);
            if (text_input_active && inset_prev >= 100 && inset < 10 && document.activeElement === text_input) {
                setTimeout(function() {
                    if (app_ptr !== 0 && text_input_active && compute_inset() < 10 && document.activeElement === text_input) {
                        wasm_exports.app_wasm_handle_text_input_dismissed(app_ptr, text_input_canvas);
                    }
                }, 250);
            }
            inset_prev = inset;
        };
        vv.addEventListener('resize', handle_vv);
        vv.addEventListener('scroll', handle_vv);
    }

    window.addEventListener('focus', function() {
        if (text_input_active && text_input) {
            text_input.focus({ preventScroll: true });
        }
    });

    window.addEventListener('resize', function() {
        canvases.forEach(canvas_apply_rect);
    });

    canvas_pixel_ratio_watch();
}

const env = {
    cr_js_now() {
        return now_ms();
    },
    cr_js_thread_count() {
        return thread_count;
    },
    cr_js_check_fail(message) {
        const error = new Error('uniform: ' + cstr(message));
        error.check = true;
        throw error;
    },
    app_js_init: app_js_init,
    app_js_canvas_create: app_js_canvas_create,
    app_js_canvas_destroy(id) {
        const entry = canvas_get(id);
        if (!entry) {
            return;
        }
        if (text_input_canvas === id && text_input) {
            text_input.blur();
            text_input_active = false;
            text_input_canvas = 0;
        }
        if (entry.ro) {
            entry.ro.disconnect();
        }
        entry.el.remove();
        canvases.delete(id);
    },
    app_js_canvas_set_rect(id, x, y, w, h) {
        const entry = canvas_get(id);
        entry.x = x;
        entry.y = y;
        entry.w = w;
        entry.h = h;
        entry.fill = !(w > 0 && h > 0);
        canvas_apply_rect(entry);
    },
    app_js_canvas_set_hidden(id, hidden) {
        canvas_get(id).el.style.visibility = hidden ? 'hidden' : 'visible';
    },
    app_js_canvas_order_front(id) {
        const el = canvas_get(id).el;
        const focused = document.activeElement === el;
        document.body.appendChild(el);
        if (focused) {
            el.focus({ preventScroll: true });
        }
    },
    app_js_canvas_order_back(id) {
        const el = canvas_get(id).el;
        const focused = document.activeElement === el;
        document.body.insertBefore(el, document.body.firstChild);
        if (focused) {
            el.focus({ preventScroll: true });
        }
    },
    app_js_canvas_focus(id) {
        canvas_get(id).el.focus({ preventScroll: true });
    },
    app_js_canvas_x(id) {
        return canvas_get(id).el.getBoundingClientRect().left;
    },
    app_js_canvas_y(id) {
        return canvas_get(id).el.getBoundingClientRect().top;
    },
    app_js_canvas_sync_size(id) {
        canvas_push_size(canvas_get(id));
    },
    app_js_canvas_set_size(id, width, height) {
        const el = canvas_get(id).el;
        el.width = width;
        el.height = height;
    },
    app_js_canvas_set_cursor(id, cursor) {
        canvas_get(id).el.style.cursor = cursor === cursor_type.text ? 'text' : 'default';
    },
    app_js_canvas_set_title(id, title) {
        if (canvases.size === 1) {
            document.title = cstr(title);
        }
    },
    app_js_screen_width() {
        return window.innerWidth;
    },
    app_js_screen_height() {
        return window.innerHeight;
    },
    app_js_is_dark() {
        return window.matchMedia('(prefers-color-scheme: dark)').matches ? 1 : 0;
    },
    app_js_text_input_set_active(id, active) {
        text_input_active = !!active;
        if (!text_input) {
            return;
        }
        if (text_input_active) {
            text_input_canvas = id;
            text_input_reset(text_input);
            text_input.focus({ preventScroll: true });
        } else if (text_input_canvas === id) {
            text_input.blur();
            text_input.value = '';
            text_input_canvas = 0;
        }
    },
    app_js_text_input_set_caret_rect(id, x, y, height) {
        if (!text_input) {
            return;
        }
        const rect = canvas_get(id).el.getBoundingClientRect();
        text_input.style.left = Math.round(rect.left + x) + 'px';
        text_input.style.top = Math.round(rect.top + y) + 'px';
        text_input.style.height = Math.max(4, Math.round(height)) + 'px';
    },
    app_js_set_clipboard(ptr, size) {
        const str = text_decoder.decode(mu8().subarray(ptr, ptr + size));
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(str).catch(function() {});
        }
    },
    app_js_animation_frame_loop(callback, user) {
        const cb = wasm_table.get(callback);
        function step(timestamp) {
            if (cb(timestamp, user)) {
                requestAnimationFrame(step);
            } else {
                app_ptr = 0;
            }
        }
        requestAnimationFrame(step);
    },
    cr_js_fetch(url, callback, user) {
        const cb = wasm_table.get(callback);
        fetch(new URL(cstr(url), base_url)).then((response) => response.ok ? response.arrayBuffer() : null).catch(() => null).then((buffer) => {
            const size = buffer ? buffer.byteLength : 0;
            const data = size ? wasm_exports.malloc(size) : 0;
            if (data) {
                mu8().set(new Uint8Array(buffer), data);
            }
            cb(user, data, data ? size : 0);
        });
    },
    cr_js_thread_free_count() {
        let count = 0;
        for (let i = 0; i < pool_flags.length; i++) {
            if (Atomics.load(pool_flags, i) === 0) {
                count++;
            }
        }
        return count;
    },
    ...wgpu,
};

function wasm_memory_limits(bytes) {
    const view = new DataView(bytes);
    let pos = 8;
    const u32 = () => {
        let result = 0;
        let shift = 0;
        let byte;
        do {
            byte = view.getUint8(pos++);
            result += (byte & 0x7f) * 2 ** shift;
            shift += 7;
        } while (byte & 0x80);
        return result;
    };
    const limits = () => {
        const flags = view.getUint8(pos++);
        const initial = u32();
        const maximum = flags & 1 ? u32() : 32768;
        return { initial: initial, maximum: maximum };
    };
    while (pos < view.byteLength) {
        const id = view.getUint8(pos++);
        const end = u32() + pos;
        if (id === 2) {
            const count = u32();
            for (let i = 0; i < count; i++) {
                const module_size = u32();
                pos += module_size;
                const field_size = u32();
                pos += field_size;
                const kind = view.getUint8(pos++);
                if (kind === 2) {
                    return limits();
                } else if (kind === 1) {
                    pos++;
                    limits();
                } else if (kind === 3) {
                    pos += 2;
                } else if (kind === 4) {
                    pos++;
                    u32();
                } else {
                    u32();
                }
            }
        }
        pos = end;
    }
    return { initial: 256, maximum: 32768 };
}

const worker_pool = [];
let pool_ready = null;
let pool_flags = is_worker ? null : new Int32Array(new SharedArrayBuffer(thread_count * 4)).fill(1);

function pool_worker_url() {
    if (new URL(script_url).origin === location.origin) {
        return script_url;
    }
    return URL.createObjectURL(new Blob(['importScripts(' + JSON.stringify(script_url) + ');'], { type: 'text/javascript' }));
}

function pool_start() {
    const url = pool_worker_url();
    for (let i = 0; i < thread_count; i++) {
        const worker = new Worker(url);
        worker.onerror = (e) => fatal('worker: ' + e.message);
        worker_pool.push(worker);
    }
}

function pool_init() {
    for (let i = 0; i < worker_pool.length; i++) {
        worker_pool[i].postMessage({ module: wasm_module, memory: wasm_memory, time_origin: time_origin, pool_flags: pool_flags.buffer, pool_index: i, base_url: base_url, remote_log: remote_log });
    }
    pool_ready = new Promise((resolve) => {
        const poll = () => env.cr_js_thread_free_count() === thread_count ? resolve() : setTimeout(poll, 1);
        poll();
    });
}

const wasi_threads = {
    'thread-spawn'(start_arg) {
        for (let i = 0; i < worker_pool.length; i++) {
            if (Atomics.compareExchange(pool_flags, i, 0, 1) === 0) {
                const tid = thread_next_id++;
                worker_pool[i].postMessage({ tid: tid, start_arg: start_arg });
                return tid;
            }
        }
        throw new Error('thread-spawn: thread pool exhausted');
    },
};

function wasm_imports() {
    env.memory = wasm_memory;
    return { env: env, wasi_snapshot_preview1: wasi, wasi: wasi_threads };
}

function boot_args() {
    const name = location.pathname.split('/').filter((t) => t.length > 0).pop() || 'app';
    return [name].concat(location.search.length > 1
        ? location.search.slice(1).split('&').filter((t) => t.length > 0).map(decodeURIComponent)
        : []);
}

function boot_main() {
    const args = boot_args();
    const argv = wasm_exports.malloc(args.length * 4);
    args.forEach((arg, i) => {
        const ptr = cstr_alloc(arg);
        new DataView(wasm_memory.buffer).setUint32(argv + i * 4, ptr, true);
    });
    (wasm_exports.main || wasm_exports.__main_argc_argv)(args.length, argv);
}

if (is_worker) {
    let pool_index = 0;
    let boot = null;
    self.addEventListener('unhandledrejection', (e) => fatal(e.reason));
    self.onmessage = (e) => {
        const d = e.data;
        if (!boot) {
            wasm_module = d.module;
            wasm_memory = d.memory;
            time_origin = d.time_origin;
            pool_flags = new Int32Array(d.pool_flags);
            pool_index = d.pool_index;
            base_url = d.base_url;
            remote_log = d.remote_log;
            boot = WebAssembly.instantiate(wasm_module, wasm_imports()).then((instance) => {
                wasm_exports = instance.exports;
                wasm_table = wasm_exports.__indirect_function_table;
                Atomics.store(pool_flags, pool_index, 0);
            });
            return;
        }
        boot = boot.then(() => {
            thread_next_id = d.tid << 20;
            wasm_exports.wasi_thread_start(d.tid, d.start_arg);
            Atomics.store(pool_flags, pool_index, 0);
        });
    };
} else {
    window.addEventListener('error', (e) => {
        e.preventDefault();
        uncaught(e.error || e.message);
    });
    window.addEventListener('unhandledrejection', (e) => {
        e.preventDefault();
        uncaught(e.reason);
    });
    pool_start();
    fetch(wasm_url).then((response) => response.arrayBuffer()).then((bytes) => {
        const limits = wasm_memory_limits(bytes);
        wasm_memory = new WebAssembly.Memory({ initial: limits.initial, maximum: limits.maximum, shared: true });
        return WebAssembly.compile(bytes);
    }).then((module) => {
        wasm_module = module;
        return WebAssembly.instantiate(module, wasm_imports());
    }).then((instance) => {
        wasm_exports = instance.exports;
        wasm_table = wasm_exports.__indirect_function_table;
        wasm_exports._initialize();
        window.wasm_exports = wasm_exports;
        pool_init();
    }).then(typeof web_start === 'function' ? web_start : boot_main).catch(fatal);
}
