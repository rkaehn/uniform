#include "uniform.h"
#include "js_internal.h"
#include "jsc_gen.h"
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char* const runtime_js_data[];
extern const unsigned long long runtime_js_sizes[];

static struct uniform_jsc {
    JSGlobalContextRef ctx;
    JSObjectRef dispatch;
    JSObjectRef on_timer;
    struct cr_str* dir;
    struct cr_str* resources;
    int argc;
    char** argv;
    uint32_t timer_count;
    uint32_t fetch_count;
    bool app_requested;
    struct app_desc app_desc;
} uniform_jsc;

static char* uniform_utf8(JSValueRef value, size_t* out_size) {
    JSStringRef string = JSValueToStringCopy(uniform_jsc.ctx, value, NULL);
    if (string == NULL) {
        *out_size = 0;
        return calloc(1, 1);
    }
    size_t capacity = JSStringGetMaximumUTF8CStringSize(string);
    char* bytes = malloc(capacity);
    *out_size = JSStringGetUTF8CString(string, bytes, capacity) - 1;
    JSStringRelease(string);
    return bytes;
}

static void uniform_print(FILE* file, JSValueRef value) {
    size_t size = 0;
    char* text = uniform_utf8(value, &size);
    fwrite(text, 1, size, file);
    free(text);
}

static JSValueRef uniform_string_value(const char* utf8) {
    JSStringRef string = JSStringCreateWithUTF8CString(utf8);
    JSValueRef value = JSValueMakeString(uniform_jsc.ctx, string);
    JSStringRelease(string);
    return value;
}

static JSValueRef uniform_get(JSObjectRef object, const char* name) {
    JSStringRef string = JSStringCreateWithUTF8CString(name);
    JSValueRef value = JSObjectGetProperty(uniform_jsc.ctx, object, string, NULL);
    JSStringRelease(string);
    return value;
}

static void uniform_set(JSObjectRef object, const char* name, JSValueRef value) {
    JSStringRef string = JSStringCreateWithUTF8CString(name);
    JSObjectSetProperty(uniform_jsc.ctx, object, string, value, kJSPropertyAttributeNone, NULL);
    JSStringRelease(string);
}

static void uniform_set_function(JSObjectRef object, const char* name, JSObjectCallAsFunctionCallback callback) {
    JSStringRef string = JSStringCreateWithUTF8CString(name);
    JSObjectSetProperty(uniform_jsc.ctx, object, string, JSObjectMakeFunctionWithCallback(uniform_jsc.ctx, string, callback), kJSPropertyAttributeNone, NULL);
    JSStringRelease(string);
}

static JSObjectRef uniform_function(JSObjectRef object, const char* name) {
    JSObjectRef function = JSValueToObject(uniform_jsc.ctx, uniform_get(object, name), NULL);
    JSValueProtect(uniform_jsc.ctx, function);
    return function;
}

struct jsc_guard* jsc_guard_top;
static const char* uniform_check_message;

static void uniform_check_handler(const char* message) {
    if (jsc_guard_top != NULL) {
        uniform_check_message = message;
        _longjmp(jsc_guard_top->jmp, 1);
    }
}

JSValueRef jsc_guard_throw(JSContextRef ctx, struct jsc_guard* guard, JSValueRef* exception) {
    jsc_guard_pop(guard);
    char text[1280];
    snprintf(text, sizeof(text), "uniform: %s", uniform_check_message);
    JSValueRef message = uniform_string_value(text);
    JSObjectRef error = JSObjectMakeError(ctx, 1, &message, NULL);
    uniform_set(error, "check", JSValueMakeBoolean(ctx, true));
    *exception = error;
    return NULL;
}

__attribute__((noreturn)) static void uniform_fail(JSValueRef exception) {
    JSContextRef ctx = uniform_jsc.ctx;
    JSValueRef stack = NULL;
    if (JSValueIsObject(ctx, exception)) {
        JSObjectRef error = JSValueToObject(ctx, exception, NULL);
        JSValueRef url = uniform_get(error, "sourceURL");
        if (JSValueIsString(ctx, url)) {
            uniform_print(stderr, url);
            fputc(':', stderr);
            uniform_print(stderr, uniform_get(error, "line"));
            fputs(": ", stderr);
        }
        stack = uniform_get(error, "stack");
    }
    uniform_print(stderr, exception);
    if (stack != NULL && JSValueIsString(ctx, stack)) {
        fputc('\n', stderr);
        uniform_print(stderr, stack);
    }
    fputc('\n', stderr);
    exit(1);
}

static void uniform_wake(void) {
    if (uniform_jsc.app_requested == false) {
        CFRunLoopStop(CFRunLoopGetMain());
    }
}

static void uniform_call(JSObjectRef function, size_t argc, const JSValueRef argv[]) {
    JSValueRef exception = NULL;
    JSObjectCallAsFunction(uniform_jsc.ctx, function, NULL, argc, argv, &exception);
    if (exception != NULL) {
        uniform_fail(exception);
    }
}

static void uniform_evaluate(const char* source, const char* url) {
    JSStringRef script = JSStringCreateWithUTF8CString(source);
    JSStringRef source_url = JSStringCreateWithUTF8CString(url);
    JSValueRef exception = NULL;
    JSEvaluateScript(uniform_jsc.ctx, script, NULL, source_url, 1, &exception);
    JSStringRelease(script);
    JSStringRelease(source_url);
    if (exception != NULL) {
        uniform_fail(exception);
    }
}

double js_dispatch(uint32_t kind, uint32_t id, double a0, double a1, double a2, double a3, double a4, double a5) {
    const double args[] = { kind, id, a0, a1, a2, a3, a4, a5 };
    memcpy(js_args + js_layout_dispatch_args, args, sizeof(args));
    uniform_call(uniform_jsc.dispatch, 0, NULL);
    return js_args[js_layout_dispatch_result];
}

static JSValueRef uniform_app_run(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    double temp[1];
    const double* a = jsc_args(ctx, argc, argv, temp, 1);
    struct jsc_guard guard;
    if (jsc_guard_enter(&guard)) {
        return jsc_guard_throw(ctx, &guard, exception);
    }
    cr_check(uniform_jsc.app_requested == false, "app.run is called once");
    uniform_jsc.app_desc = *(const struct app_desc*)js_p(a[0]);
    uniform_jsc.app_desc.argc = uniform_jsc.argc;
    uniform_jsc.app_desc.argv = uniform_jsc.argv;
    uniform_jsc.app_requested = true;
    jsc_guard_pop(&guard);
    CFRunLoopStop(CFRunLoopGetMain());
    return JSValueMakeUndefined(ctx);
}

static JSValueRef uniform_native_print(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    uniform_print(JSValueToBoolean(ctx, argv[1]) ? stderr : stdout, argv[0]);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef uniform_native_now(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    return JSValueMakeNumber(ctx, cr_get_current_time() * 1000.0);
}

static void uniform_free(void* bytes, void* context) {
    free(bytes);
}

static JSValueRef uniform_native_encode(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    size_t size = 0;
    char* bytes = uniform_utf8(argv[0], &size);
    return JSObjectMakeTypedArrayWithBytesNoCopy(ctx, kJSTypedArrayTypeUint8Array, bytes, size, uniform_free, NULL, exception);
}

static size_t uniform_utf16_from_utf8(const uint8_t* bytes, size_t size, JSChar* out) {
    size_t count = 0;
    size_t i = size >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
    while (i < size) {
        uint8_t lead = bytes[i++];
        if (lead < 0x80) {
            out[count++] = lead;
            continue;
        }
        if (lead < 0xc2 || lead > 0xf4) {
            out[count++] = 0xfffd;
            continue;
        }
        uint32_t needed = lead >= 0xf0 ? 3 : lead >= 0xe0 ? 2 : 1;
        uint8_t lower = lead == 0xe0 ? 0xa0 : lead == 0xf0 ? 0x90 : 0x80;
        uint8_t upper = lead == 0xed ? 0x9f : lead == 0xf4 ? 0x8f : 0xbf;
        uint32_t code_point = lead & (0x3fu >> needed);
        uint32_t seen = 0;
        while (seen < needed && i < size && bytes[i] >= lower && bytes[i] <= upper) {
            code_point = (code_point << 6) | (bytes[i++] & 0x3fu);
            lower = 0x80;
            upper = 0xbf;
            seen++;
        }
        if (seen < needed) {
            out[count++] = 0xfffd;
        } else if (code_point >= 0x10000) {
            out[count++] = (JSChar)(0xd800 + ((code_point - 0x10000) >> 10));
            out[count++] = (JSChar)(0xdc00 + ((code_point - 0x10000) & 0x3ff));
        } else {
            out[count++] = (JSChar)code_point;
        }
    }
    return count;
}

static JSValueRef uniform_native_decode(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    JSObjectRef array = JSValueToObject(ctx, argv[0], NULL);
    size_t size = JSObjectGetTypedArrayByteLength(ctx, array, NULL);
    const uint8_t* bytes = (const uint8_t*)JSObjectGetTypedArrayBytesPtr(ctx, array, NULL) + JSObjectGetTypedArrayByteOffset(ctx, array, NULL);
    JSChar* chars = malloc(size * sizeof(JSChar));
    JSStringRef string = JSStringCreateWithCharacters(chars, uniform_utf16_from_utf8(bytes, size, chars));
    JSValueRef result = JSValueMakeString(ctx, string);
    JSStringRelease(string);
    free(chars);
    return result;
}

static JSValueRef uniform_native_set_timer(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    double id = JSValueToNumber(ctx, argv[0], NULL);
    double seconds = JSValueToNumber(ctx, argv[1], NULL) / 1000.0;
    bool repeat = JSValueToBoolean(ctx, argv[2]);
    CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + seconds, repeat ? fmax(seconds, 0.001) : 0.0, 0, 0, ^(CFRunLoopTimerRef fired) {
        if (repeat == false) {
            uniform_jsc.timer_count--;
        }
        JSValueRef args[] = { JSValueMakeNumber(uniform_jsc.ctx, id) };
        uniform_call(uniform_jsc.on_timer, 1, args);
        uniform_wake();
    });
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    CFRelease(timer);
    uniform_jsc.timer_count++;
    return JSValueMakeNumber(ctx, js_ptr(timer));
}

static JSValueRef uniform_native_clear_timer(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    CFRunLoopTimerInvalidate(js_p(JSValueToNumber(ctx, argv[0], NULL)));
    uniform_jsc.timer_count--;
    return JSValueMakeUndefined(ctx);
}

static void uniform_fetch_free(void* bytes, void* context) {
    cr_str_destroy(context);
}

static void uniform_fetch_finish(JSObjectRef done, int status, struct cr_str* body, const char* error) {
    JSContextRef ctx = uniform_jsc.ctx;
    if (body == NULL) {
        body = cr_str_create_empty(cr_mem_libc);
    }
    JSValueRef args[] = {
        JSValueMakeNumber(ctx, status),
        JSObjectMakeArrayBufferWithBytesNoCopy(ctx, cr_str_get_raw(body), cr_str_get_count(body), uniform_fetch_free, body, NULL),
        uniform_string_value(error != NULL ? error : ""),
    };
    uniform_jsc.fetch_count--;
    uniform_call(done, 3, args);
    JSValueUnprotect(ctx, done);
    uniform_wake();
}

static struct cr_str* uniform_read_relative(struct cr_str* dir, struct cr_str* relative) {
    struct cr_str* path = cr_str_create_copy(cr_mem_libc, dir);
    cr_str_append_path_component(path, relative);
    struct cr_str* body = cr_str_create_with_contents_of_file(cr_mem_libc, path);
    cr_str_destroy(path);
    return body;
}

static JSValueRef uniform_native_fetch(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    size_t size = 0;
    char* url = uniform_utf8(argv[0], &size);
    JSObjectRef done = JSValueToObject(ctx, argv[1], NULL);
    JSValueProtect(ctx, done);
    uniform_jsc.fetch_count++;
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0) {
        NSURL* ns_url = [NSURL URLWithString:@(url)];
        if (ns_url == nil) {
            uniform_fetch_finish(done, 0, NULL, "invalid URL");
        } else {
            [[NSURLSession.sharedSession dataTaskWithURL:ns_url completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
                int status = error != nil ? 0 : [response isKindOfClass:NSHTTPURLResponse.class] ? (int)((NSHTTPURLResponse*)response).statusCode : 200;
                struct cr_str* body = data != nil ? cr_str_create_data(cr_mem_libc, data.bytes, (uint32_t)data.length) : NULL;
                dispatch_async(dispatch_get_main_queue(), ^{
                    uniform_fetch_finish(done, status, body, error.localizedDescription.UTF8String);
                });
            }] resume];
        }
    } else {
        uint8_t raw[cr_str_size];
        struct cr_str* relative = cr_str_temp(raw, url);
        struct cr_str* body;
        if (cr_str_is_absolute_path(relative)) {
            body = cr_str_create_with_contents_of_file(cr_mem_libc, relative);
        } else {
            body = uniform_read_relative(uniform_jsc.dir, relative);
            if (body == NULL && uniform_jsc.resources != NULL) {
                body = uniform_read_relative(uniform_jsc.resources, relative);
            }
        }
        uniform_fetch_finish(done, body != NULL ? 200 : 404, body, NULL);
    }
    free(url);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef uniform_native_exit(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
    exit((int)JSValueToNumber(ctx, argv[0], NULL));
}

static JSObjectRef uniform_native_create(const char* script_path, char** args, int arg_count) {
    JSContextRef ctx = uniform_jsc.ctx;
    JSObjectRef native = JSObjectMake(ctx, NULL, NULL);
    uniform_set_function(native, "print", uniform_native_print);
    uniform_set_function(native, "now", uniform_native_now);
    uniform_set_function(native, "encode", uniform_native_encode);
    uniform_set_function(native, "decode", uniform_native_decode);
    uniform_set_function(native, "setTimer", uniform_native_set_timer);
    uniform_set_function(native, "clearTimer", uniform_native_clear_timer);
    uniform_set_function(native, "fetch", uniform_native_fetch);
    uniform_set_function(native, "exit", uniform_native_exit);

    JSObjectRef exports = JSObjectMake(ctx, NULL, NULL);
    for (const struct jsc_entry* entry = jsc_table; entry->name != NULL; entry++) {
        uniform_set_function(exports, entry->name, strcmp(entry->name, "app_run") == 0 ? uniform_app_run : entry->callback);
    }
    uniform_set(native, "exports", exports);

    JSObjectRef script_args = JSObjectMakeArray(ctx, 0, NULL, NULL);
    JSObjectSetPropertyAtIndex(ctx, script_args, 0, uniform_string_value(script_path), NULL);
    for (int i = 0; i < arg_count; i++) {
        JSObjectSetPropertyAtIndex(ctx, script_args, (unsigned)i + 1, uniform_string_value(args[i]), NULL);
    }
    uniform_set(native, "args", script_args);

    uniform_set(native, "buffer", JSObjectMakeArrayBufferWithBytesNoCopy(ctx, js_arena, js_layout_scratch_size + js_layout_bump_size, NULL, NULL, NULL));
    uniform_set(native, "base", JSValueMakeNumber(ctx, js_ptr(js_arena)));
    return native;
}

static struct cr_str* uniform_read_stdin(void) {
    struct cr_str* script = cr_str_create_empty(cr_mem_libc);
    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
        cr_str_append_data(script, buffer, (uint32_t)count);
    }
    return script;
}

int uniform_run(const char* script_path, int argc, char** argv, int script_args) {
    struct uniform_jsc* js = &uniform_jsc;

    bool from_stdin = strcmp(script_path, "-") == 0;
    uint8_t raw[cr_str_size];
    struct cr_str* path = from_stdin ? cr_str_create(cr_mem_libc, "-") : cr_util_get_real_path(cr_mem_libc, cr_str_temp(raw, (char*)script_path));
    struct cr_str* script = from_stdin ? uniform_read_stdin() : path != NULL ? cr_str_create_with_contents_of_file(cr_mem_libc, path) : NULL;
    if (script == NULL) {
        fprintf(stderr, "uniform: cannot read %s\n", script_path);
        return 1;
    }

    js->argc = argc;
    js->argv = argv;
    if (from_stdin) {
        js->dir = cr_util_get_working_dir(cr_mem_libc);
    } else {
        js->dir = cr_str_create_copy(cr_mem_libc, path);
        cr_str_remove_last_path_component(js->dir);
    }
    js->resources = cr_util_get_resources_dir(cr_mem_libc);

    js_arena_init();
    cr_check_handler = uniform_check_handler;
    js->ctx = JSGlobalContextCreate(NULL);

    cr_autoreleasepool {
        JSObjectRef native = uniform_native_create(cr_str_get_data(path), argv + script_args, argc - script_args);
        uniform_set(JSContextGetGlobalObject(js->ctx), "__js_native", native);
        uniform_evaluate((const char*)runtime_js_data[0], "uniform.js");
        js->dispatch = uniform_function(native, "dispatch");
        js->on_timer = uniform_function(native, "onTimer");
        uniform_evaluate(cr_str_get_data(script), cr_str_get_data(path));
    }
    cr_str_destroy(script);
    cr_str_destroy(path);

    while (js->app_requested == false && (js->timer_count > 0 || js->fetch_count > 0)) {
        cr_autoreleasepool {
            CFRunLoopRun();
        }
    }

    if (js->app_requested) {
        app_run(&js->app_desc);
    }
    return 0;
}
