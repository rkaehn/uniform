'use strict';

function uniform_alloc(size) {
    js_assert(typeof size === 'number' && size > 0, 'uniform.alloc expects a byte count');
    return js_x.js_malloc(size);
}

function uniform_pointer(api, ptr) {
    js_assert(typeof ptr === 'number' && ptr >= 0 && Number.isInteger(ptr), api + ' expects a pointer');
    return ptr;
}

function uniform_free(ptr) {
    if (uniform_pointer('uniform.free', ptr) !== 0) {
        js_cb_replace_release(ptr);
        js_x.js_free(ptr);
    }
}

function uniform_read(ptr, len) {
    js_assert(typeof len === 'number' && len >= 0 && Number.isInteger(len), 'uniform.read expects a byte count');
    return js_host_read(uniform_pointer('uniform.read', ptr), len);
}

function uniform_write(ptr, bytes) {
    js_host_write(uniform_pointer('uniform.write', ptr), bytes);
}

function uniform_struct(api, name) {
    const entry = js_structs[name];
    if (entry === undefined) {
        throw js_error(api + ': unknown struct ' + name);
    }
    return entry;
}

function uniform_sizeof(name) {
    return uniform_struct('uniform.sizeof', name)[2];
}

function uniform_read_struct(name, ptr) {
    const entry = uniform_struct('uniform.readStruct', name);
    const m = js_scratch_cursor;
    const result = entry[1](js_host_copy(uniform_pointer('uniform.readStruct', ptr), entry[2]));
    js_scratch_cursor = m;
    return result;
}

function uniform_write_struct(name, ptr, value) {
    const entry = uniform_struct('uniform.writeStruct', name);
    const m = js_scratch_cursor;
    const off = js_scratch(entry[2]);
    js_u8.fill(0, off, off + entry[2]);
    entry[0](off, value);
    js_host_write(uniform_pointer('uniform.writeStruct', ptr), js_u8.subarray(off, off + entry[2]));
    js_scratch_cursor = m;
}

const js_handle_buffer = new ArrayBuffer(8);
const js_handle_f64 = new Float64Array(js_handle_buffer);
const js_handle_u32 = new Uint32Array(js_handle_buffer);

function ui_handle_make(gen, idx) {
    js_handle_u32[0] = ~idx >>> 0;
    js_handle_u32[1] = gen >>> 0;
    return js_handle_f64[0];
}

function ui_handle_get_gen(handle) {
    js_handle_f64[0] = handle;
    return js_handle_u32[1];
}

function ui_handle_get_idx(handle) {
    js_handle_f64[0] = handle;
    return ~js_handle_u32[0] >>> 0;
}

function ui_handle_is_null(handle) {
    return handle === 0 || ui_handle_get_idx(handle) === 0xffffffff;
}

function ui_handle_is_equal(a, b) {
    return a === b;
}

function ui_key(value) {
    const m = js_scratch_cursor;
    const key = js_r_ui_key(js_pone_ui_key(value) - js_base);
    js_scratch_cursor = m;
    return key;
}

function ui_color(value, colorSpace) {
    return { value: typeof value === 'number' ? [value, value, value, value] : value, colorSpace: colorSpace };
}

function ui_rgba_cs(r, g, b, a, colorSpace) {
    return { value: [r, g, b, a], colorSpace: colorSpace };
}

function ui_rgba(r, g, b, a) {
    return { value: [r, g, b, a], colorSpace: 'srgb' };
}

function ui_hex_a(hex, alpha) {
    return ui_rgba(((hex >>> 16) & 255) / 255, ((hex >>> 8) & 255) / 255, (hex & 255) / 255, alpha);
}

function ui_hex(hex) {
    return ui_hex_a(hex, 1);
}

function ui_gray_a(x, alpha) {
    return ui_rgba(x, x, x, alpha);
}

function ui_gray(x) {
    return ui_rgba(x, x, x, 1);
}

function ui_pad(x) {
    return [x, x, x, x];
}

function ui_pad_xy(x, y) {
    return [x, x, y, y];
}

function ui_pos_axis_auto() {
    return { mode: 'auto' };
}

function ui_pos_axis_rel(anchor, parentAnchor, constant) {
    return { mode: 'rel', anchor: anchor, parentAnchor: parentAnchor, constant: constant };
}

function ui_pos_axis_start() {
    return ui_pos_axis_rel('start', 'start', 0);
}

function ui_pos_axis_center() {
    return ui_pos_axis_rel('center', 'center', 0);
}

function ui_pos_axis_end() {
    return ui_pos_axis_rel('end', 'end', 0);
}

function ui_pos(x, y) {
    return { x: x, y: y };
}

function ui_pos_xy(x, y) {
    return ui_pos(ui_pos_axis_rel('start', 'start', x), ui_pos_axis_rel('start', 'start', y));
}

function ui_pos_center() {
    return ui_pos(ui_pos_axis_center(), ui_pos_axis_center());
}

function ui_size_axis_fit() {
    return { mode: 'fit' };
}

function ui_size_axis_fill() {
    return { mode: 'fill' };
}

function ui_size_axis_fix(constant) {
    return { mode: 'fix', constant: constant };
}

function ui_size_axis_rel(anchor, multiplier, constant) {
    return { mode: 'rel', anchor: anchor, multiplier: multiplier, constant: constant };
}

function ui_size_axis_width() {
    return ui_size_axis_rel('width', 1, 0);
}

function ui_size_axis_height() {
    return ui_size_axis_rel('height', 1, 0);
}

function ui_size(width, height) {
    return { width: width, height: height };
}

function ui_size_fix(width, height) {
    return ui_size(ui_size_axis_fix(width), ui_size_axis_fix(height));
}

function ui_size_fit() {
    return ui_size(ui_size_axis_fit(), ui_size_axis_fit());
}

function ui_size_fill() {
    return ui_size(ui_size_axis_fill(), ui_size_axis_fill());
}

function ui_layout_stack(mode, dist, align, spacing, padding) {
    return { mode: mode, dist: dist, align: align, spacing: spacing, padding: typeof padding === 'number' ? ui_pad(padding) : padding };
}

function ui_layout_stack_x(dist, align, spacing, padding) {
    return ui_layout_stack('stackX', dist, align, spacing, padding);
}

function ui_layout_stack_y(dist, align, spacing, padding) {
    return ui_layout_stack('stackY', dist, align, spacing, padding);
}

function ui_layer_solid(color) {
    return { type: 'solid', solid: { color: color } };
}

function ui_layer_gradient(direction, colorStart, colorEnd) {
    return { type: 'gradient', gradient: { direction: direction, colorStart: colorStart, colorEnd: colorEnd } };
}

function ui_layer_graphic(graphic, addressMode, tint) {
    return { type: 'graphic', graphic: { graphic: graphic, addressMode: addressMode, tint: tint, premultiplied: true } };
}

function ui_layer_graphic_ex(graphic, addressMode, filter, scale, tint, colorSpace, premultiplied) {
    return { type: 'graphic', graphic: { graphic: graphic, addressMode: addressMode, filter: filter, scale: typeof scale === 'number' ? [scale, scale] : scale, tint: tint, colorSpace: colorSpace, premultiplied: premultiplied } };
}

function ui_layer_background_blur(radius) {
    return { type: 'backgroundBlur', backgroundBlur: { radius: radius } };
}

function ui_layer_glass(variant, appearance) {
    return { type: 'glass', glass: { variant: variant, appearance: appearance, slider: ui.glassSliderDefault } };
}

function ui_layer_glass_ex(variant, appearance, tint, slider) {
    return { type: 'glass', glass: { variant: variant, appearance: appearance, tint: tint, slider: slider } };
}

function ui_layer_opacity(layer, opacity) {
    return Object.assign({}, layer, { blend: { mode: 'normal', opacity: opacity } });
}

function ui_fill_solid(color) {
    return { layers: [ui_layer_solid(color)] };
}

function ui_fill_gradient(direction, colorStart, colorEnd) {
    return { layers: [ui_layer_gradient(direction, colorStart, colorEnd)] };
}

function ui_fill_graphic(graphic, addressMode, tint) {
    return { layers: [ui_layer_graphic(graphic, addressMode, tint)] };
}

function ui_outline_solid(align, thickness, color) {
    return { align: align, thickness: thickness, layers: [ui_layer_solid(color)] };
}

function ui_shadow(x, y, radius, color) {
    return { offset: [x, y], radius: radius, color: color };
}

function ui_blur(radius) {
    return { radius: radius };
}

function ui_corners(radius) {
    return { radius: [radius, radius, radius, radius] };
}

function ui_corners_circular(radius) {
    return { profile: 'circular', radius: [radius, radius, radius, radius] };
}

function ui_str(raw, color, ptSize) {
    return { raw: raw, attributes: [{ start: 0, end: 4294967295, type: 'color', color: color }, { start: 0, end: 4294967295, type: 'ptSize', ptSize: ptSize }] };
}

function ui_str_weight(raw, color, ptSize, weight) {
    return { raw: raw, attributes: [{ start: 0, end: 4294967295, type: 'color', color: color }, { start: 0, end: 4294967295, type: 'ptSize', ptSize: ptSize }, { start: 0, end: 4294967295, type: 'weight', weight: weight }] };
}

function ui_color_space_from_app(colorSpace) {
    return js_enum(js_enums.app_color_space, colorSpace) === js_enums.app_color_space.displayP3 ? 'displayP3' : 'srgb';
}

function ui_pixel_format_from_app(pixelFormat) {
    return js_enum(js_enums.app_pixel_format, pixelFormat) === js_enums.app_pixel_format.rgba16f ? 'rgba16f' : 'bgra8';
}

function ui_pan_axis(direction) {
    return js_enum(js_enums.ui_pan_direction, direction) === js_enums.ui_pan_direction.x ? 0 : 1;
}

function ui_slide_on_pan(state, gesture) {
    const velocity = ui.panGestureIsMoving(gesture) ? ui.panGestureGetVelocity(gesture)[0] : 0;
    ui.slideOnDrag(state, ui.panGestureGetPhase(gesture), ui.panGestureGetTranslation(gesture, true)[0], velocity);
}

function ui_scroll_on_pan(state, gesture, direction) {
    const axis = ui_pan_axis(direction);
    const velocity = ui.panGestureIsMoving(gesture) ? ui.panGestureGetVelocity(gesture)[axis] : 0;
    return ui.scrollOnDrag(state, ui.panGestureGetPhase(gesture), ui.panGestureGetTranslation(gesture, true)[axis], velocity, ui.panGestureGetTimestamp(gesture));
}

function hud_theme(isDark) {
    if (isDark) {
        return { labelPrimary: ui.gray90, backgroundPrimary: ui_gray(0.08), backgroundSecondary: ui_gray(0.16), highlight: ui.gray60, blue: ui_hex(0x0076ff), font: 0 };
    }
    return { labelPrimary: ui.gray30, backgroundPrimary: ui_gray(0.96), backgroundSecondary: ui_gray(0.88), highlight: ui.white, blue: ui_hex(0x0a8cff), font: 0 };
}

const js_fonts = new Map();

function js_font_release(font, data) {
    js_fonts.delete(font);
    js_x.js_free(data);
}

function ui_font_create(bytes) {
    const u8 = js_bytes_view(bytes);
    js_assert(u8.length > 0, 'ui.fontCreate expects the font file bytes');
    const data = js_x.js_malloc(u8.length);
    js_host_write(data, u8);
    const font = js_x.ui_font_create(data, u8.length);
    js_fonts.set(font, data);
    return font;
}

function ui_font_destroy(font) {
    js_x.ui_font_destroy(js_num(font));
    const data = js_fonts.get(font);
    if (data !== undefined) {
        js_font_release(font, data);
    }
}

function ui_deinit() {
    for (const [font, data] of js_fonts) {
        js_font_release(font, data);
    }
    js_f_ui_deinit();
}

function ui_list_linear_layout(linear) {
    const layout = js_f_ui_list_linear_layout(linear);
    if (typeof linear !== 'number') {
        layout.user = 0;
        layout.linear = linear;
    }
    return layout;
}

function js_w_ui_list_layout(o, d) {
    if (d != null && d.linear !== undefined) {
        js_x.ui_list_linear_layout(js_pone_ui_list_linear(d.linear), js_base + o);
    } else {
        js_w_ui_list_layout_fields(o, d);
    }
}

function js_write_matrix(m) {
    const p = js_scratch(24);
    if (m == null) {
        js_f32.set([1, 0, 0, 1, 0, 0], p >> 2);
    } else {
        js_f32.set(m, p >> 2);
    }
    return js_base + p;
}

function js_write_vertices(vertices, floats, writer) {
    if (vertices instanceof Float32Array) {
        const count = (vertices.length / floats) | 0;
        const p = js_scratch(count * floats * 4);
        js_f32.set(count * floats === vertices.length ? vertices : vertices.subarray(0, count * floats), p >> 2);
        js_last_len = count;
        return js_base + p;
    }
    return writer(vertices);
}

function ui_custom_draw_solid(info, transform, color, vertices) {
    const m = js_scratch_cursor;
    const t = js_write_matrix(transform);
    const c = js_pone_ui_color(color);
    const v = js_write_vertices(vertices, 4, js_pa_ui_custom_vertex_solid);
    js_x.js_ui_custom_draw_solid(info.drawSolid, t, c, js_last_len, v);
    js_scratch_cursor = m;
}

function ui_custom_draw_graphic(info, transform, graphic, tint, vertices) {
    const m = js_scratch_cursor;
    const t = js_write_matrix(transform);
    const c = js_pone_ui_color(tint);
    const v = js_write_vertices(vertices, 6, js_pa_ui_custom_vertex_graphic);
    js_x.js_ui_custom_draw_graphic(info.drawGraphic, t, js_num(graphic), c, js_last_len, v);
    js_scratch_cursor = m;
}

function js_app_desc_wrap(desc) {
    js_app_desc = desc;
    return { ...desc, onStart: (a) => {
        js_app_handle = a;
        if (desc.onStart !== undefined) {
            desc.onStart(a);
        }
    } };
}

function app_run(desc) {
    js_f_app_run(js_app_desc_wrap(desc));
}

function gpu_command_buffer_create(device, label) {
    const commandBuffer = js_f_gpu_command_buffer_create(device, label);
    if (commandBuffer !== 0) {
        js_gpu_pending.push(commandBuffer);
    }
    return commandBuffer;
}

function js_gpu_forget(commandBuffer) {
    const index = js_gpu_pending.lastIndexOf(commandBuffer);
    if (index >= 0) {
        js_gpu_pending.splice(index, 1);
    }
}

function gpu_command_buffer_commit(commandBuffer) {
    const serial = js_f_gpu_command_buffer_commit(commandBuffer);
    js_gpu_forget(commandBuffer);
    return serial;
}

function gpu_command_buffer_discard(commandBuffer) {
    js_f_gpu_command_buffer_discard(commandBuffer);
    js_gpu_forget(commandBuffer);
}

function app_window_present(window, commandBuffer, target) {
    const serial = js_f_app_window_present(window, commandBuffer, target);
    js_gpu_forget(commandBuffer);
    return serial;
}

const js_windows = new Map();

function js_window_release(window, user) {
    js_windows.delete(window);
    js_cb_release(user);
    js_cb_replace_release(window);
}

function app_window_create(a, desc) {
    const window = js_f_app_window_create(a, desc);
    if (window !== 0) {
        js_windows.set(window, js_f_app_window_get_user(window));
    }
    return window;
}

function app_window_destroy(window) {
    const user = js_windows.get(window);
    js_f_app_window_destroy(window);
    js_window_release(window, user);
}

function app_restart(a, desc) {
    js_app_handle = a;
    const user = js_f_app_get_user(a);
    const windows = Array.from(js_windows);
    js_f_app_restart(a, js_app_desc_wrap(desc));
    js_cb_release(user);
    for (const [window, windowUser] of windows) {
        js_window_release(window, windowUser);
    }
}

function ui_input_set(input) {
    js_f_ui_input_set(input);
    js_input_current = js_num(input);
}

function ui_input_dispatch(root, timestamp, events, toPtFromApp) {
    js_f_ui_input_dispatch(root, timestamp, events, toPtFromApp);
    js_cb_sweep(js_input_current);
}

function ui_util_window_sync(window, prevRoot, toAppFromPt) {
    js_f_ui_util_window_sync(window, prevRoot, toAppFromPt);
    js_cb_sweep(js_input_current);
}

function ui_input_destroy(input) {
    js_f_ui_input_destroy(input);
    js_cb_release_input(input);
    if (js_num(input) === js_input_current) {
        js_input_current = 0;
    }
}

function ui_gesture(desc) {
    const destroy = desc.destroy;
    return js_f_ui_gesture({ ...desc, destroy: (gesture) => {
        if (destroy !== undefined) {
            destroy(gesture);
        }
        js_cb_disown(gesture);
    } });
}

function ui_release(root) {
    if (js_num(root) === 0) {
        return;
    }
    js_f_ui_release(root);
    js_cb_release_tree(root);
}

function log_s(str) {
    log.str(String(str));
}

uniform.alloc = uniform_alloc;
uniform.free = uniform_free;
uniform.read = uniform_read;
uniform.write = uniform_write;
uniform.sizeof = uniform_sizeof;
uniform.readStruct = uniform_read_struct;
uniform.writeStruct = uniform_write_struct;

ui.nodeNull = 0;
ui.pointerNull = 0;
ui.keyAuto = undefined;
ui.key = ui_key;
ui.nodeMake = ui_handle_make;
ui.nodeGetGen = ui_handle_get_gen;
ui.nodeGetIdx = ui_handle_get_idx;
ui.nodeIsNull = ui_handle_is_null;
ui.nodeIsEqual = ui_handle_is_equal;
ui.pointerMake = ui_handle_make;
ui.pointerGetGen = ui_handle_get_gen;
ui.pointerGetIdx = ui_handle_get_idx;
ui.pointerIsNull = ui_handle_is_null;
ui.pointerIsEqual = ui_handle_is_equal;

ui.color = ui_color;
ui.rgbaCs = ui_rgba_cs;
ui.rgba = ui_rgba;
ui.hexA = ui_hex_a;
ui.hex = ui_hex;
ui.grayA = ui_gray_a;
ui.gray = ui_gray;
ui.white = ui_rgba(1, 1, 1, 1);
ui.gray90 = ui_gray(0.9);
ui.gray80 = ui_gray(0.8);
ui.gray70 = ui_gray(0.7);
ui.gray60 = ui_gray(0.6);
ui.gray50 = ui_gray(0.5);
ui.gray40 = ui_gray(0.4);
ui.gray30 = ui_gray(0.3);
ui.gray20 = ui_gray(0.2);
ui.gray10 = ui_gray(0.1);
ui.black = ui_rgba(0, 0, 0, 1);
ui.red = ui_rgba(1, 0, 0, 1);
ui.green = ui_rgba(0, 1, 0, 1);
ui.blue = ui_rgba(0, 0, 1, 1);
ui.yellow = ui_rgba(1, 1, 0, 1);
ui.magenta = ui_rgba(1, 0, 1, 1);
ui.cyan = ui_rgba(0, 1, 1, 1);
ui.clear = ui_rgba(0, 0, 0, 0);
ui.pad = ui_pad;
ui.padXy = ui_pad_xy;
ui.posAxisAuto = ui_pos_axis_auto;
ui.posAxisRel = ui_pos_axis_rel;
ui.posAxisStart = ui_pos_axis_start;
ui.posAxisCenter = ui_pos_axis_center;
ui.posAxisEnd = ui_pos_axis_end;
ui.pos = ui_pos;
ui.posXy = ui_pos_xy;
ui.posCenter = ui_pos_center;
ui.sizeAxisFit = ui_size_axis_fit;
ui.sizeAxisFill = ui_size_axis_fill;
ui.sizeAxisFix = ui_size_axis_fix;
ui.sizeAxisRel = ui_size_axis_rel;
ui.sizeAxisWidth = ui_size_axis_width;
ui.sizeAxisHeight = ui_size_axis_height;
ui.size = ui_size;
ui.sizeFix = ui_size_fix;
ui.sizeFit = ui_size_fit;
ui.sizeFill = ui_size_fill;
ui.layoutStackX = ui_layout_stack_x;
ui.layoutStackY = ui_layout_stack_y;
ui.layerSolid = ui_layer_solid;
ui.layerGradient = ui_layer_gradient;
ui.layerGraphic = ui_layer_graphic;
ui.layerGraphicEx = ui_layer_graphic_ex;
ui.layerBackgroundBlur = ui_layer_background_blur;
ui.layerGlass = ui_layer_glass;
ui.layerGlassEx = ui_layer_glass_ex;
ui.layerOpacity = ui_layer_opacity;
ui.fillSolid = ui_fill_solid;
ui.fillGradient = ui_fill_gradient;
ui.fillGraphic = ui_fill_graphic;
ui.outlineSolid = ui_outline_solid;
ui.shadow = ui_shadow;
ui.blur = ui_blur;
ui.corners = ui_corners;
ui.cornersCircular = ui_corners_circular;
ui.str = ui_str;
ui.strWeight = ui_str_weight;
ui.colorSpaceFromApp = ui_color_space_from_app;
ui.pixelFormatFromApp = ui_pixel_format_from_app;
hud.theme = hud_theme;

ui.deinit = ui_deinit;
ui.fontCreate = ui_font_create;
ui.fontDestroy = ui_font_destroy;
ui.listLinearLayout = ui_list_linear_layout;
ui.customDrawSolid = ui_custom_draw_solid;
ui.customDrawGraphic = ui_custom_draw_graphic;
app.run = app_run;
app.restart = app_restart;
app.windowCreate = app_window_create;
app.windowDestroy = app_window_destroy;
app.windowPresent = app_window_present;
gpu.commandBufferCreate = gpu_command_buffer_create;
gpu.commandBufferCommit = gpu_command_buffer_commit;
gpu.commandBufferDiscard = gpu_command_buffer_discard;
ui.inputSet = ui_input_set;
ui.inputDispatch = ui_input_dispatch;
ui.utilWindowSync = ui_util_window_sync;
ui.inputDestroy = ui_input_destroy;
ui.gesture = ui_gesture;
ui.release = ui_release;
ui.slideOnPan = ui_slide_on_pan;
ui.scrollOnPan = ui_scroll_on_pan;
log.s = log_s;

globalThis.app = app;
globalThis.gpu = gpu;
globalThis.ui = ui;
globalThis.hud = hud;
globalThis.log = log;
globalThis.bd = bd;
