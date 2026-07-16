#include "cr.h"
#include <stdarg.h>
#include <stdio.h>

struct cr_str {
    struct cr_mem* mem;
    uint32_t capacity;
    uint32_t count;
    char* data;
};
static_assert(cr_str_size == sizeof(struct cr_str), "");

static void cr_str_ensure_capacity(struct cr_str* str, uint32_t new_capacity);
static void cr_str_insert_va(struct cr_str* str, uint32_t index, const char* fmt, va_list list);

struct cr_str* cr_str_temp(uint8_t* raw, char* cstr) {
    struct cr_str* str = (struct cr_str*)raw;

    uint32_t count = (uint32_t)strlen(cstr);

    str->mem = NULL;
    str->capacity = count + 1;
    str->count = count;
    str->data = cstr;

    return str;
}

struct cr_str* cr_str_create(struct cr_mem* mem, const char* cstr) {
    struct cr_str* str = cr_str_create_empty(mem);

    cr_str_insert_data(str, 0, cstr, (uint32_t)strlen(cstr));

    return str;
}

struct cr_str* cr_str_create_format(struct cr_mem* mem, const char* fmt, ...) {
    struct cr_str* str = cr_str_create_empty(mem);

    va_list list;
    va_start(list, fmt);
    cr_str_insert_va(str, 0, fmt, list);
    va_end(list);

    return str;
}

struct cr_str* cr_str_create_empty(struct cr_mem* mem) {
    struct cr_str* str = cr_malloc(mem, sizeof(*str), 0, cr_loc);

    str->mem = mem;
    str->capacity = 0;
    str->count = 0;
    str->data = NULL;
    cr_str_ensure_capacity(str, 1);
    str->data[0] = '\0';

    return str;
}

struct cr_str* cr_str_create_copy(struct cr_mem* mem, struct cr_str* original) {
    struct cr_str* str = cr_str_create_empty(mem);

    cr_str_append_data(str, original->data, original->count);

    return str;
}

struct cr_str* cr_str_create_data(struct cr_mem* mem, const char* data, uint32_t data_count) {
    struct cr_str* str = cr_str_create_empty(mem);

    cr_str_append_data(str, data, data_count);

    return str;
}

struct cr_str* cr_str_create_with_contents_of_file(struct cr_mem* mem, struct cr_str* path) {
    FILE* f = NULL;
    struct cr_str* str = NULL;
    bool success = false;
    f = fopen(path->data, "r");
    if (f == NULL) {
        goto end;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        goto end;
    }
    long file_size = ftell(f);
    if (file_size == -1) {
        goto end;
    }
    assert((uint64_t)file_size <= uint32_max);
    str = cr_str_create_empty(mem);
    cr_str_ensure_capacity(str, (uint32_t)file_size + 1);
    if (fseek(f, 0, SEEK_SET) != 0) {
        goto end;
    }
    uint32_t r_count = (uint32_t)fread(str->data, 1, (uint32_t)file_size, f);
    if (r_count != (uint32_t)file_size) {
        goto end;
    }
    str->count = (uint32_t)file_size;
    str->data[r_count] = '\0';
    success = true;
end:
    if (f) {
        fclose(f);
    }
    if (!success && str) {
        cr_str_destroy(str);
        str = NULL;
    }
    return str;
}

void cr_str_destroy(struct cr_str* str) {
    if (str->data) {
        cr_free(str->mem, str->data, cr_loc);
    }
    cr_free(str->mem, str, cr_loc);
}

uint32_t cr_str_get_count(struct cr_str* str) {
    return str->count;
}

const char* cr_str_get_data(struct cr_str* str) {
    return str->data;
}

char* cr_str_get_raw(struct cr_str* str) {
    return str->data;
}

bool cr_str_write_to_file(struct cr_str* str, struct cr_str* path) {
    FILE* f = NULL;
    bool success = false;
    f = fopen(path->data, "w");
    if (f == NULL) {
        goto end;
    }
    uint32_t w_count = (uint32_t)fwrite(str->data, 1, str->count, f);
    if (w_count != (uint32_t)str->count) {
        goto end;
    }
    success = true;
end:
    if (f) {
        fclose(f);
    }
    return success;
}

static void cr_str_insert_va(struct cr_str* str, uint32_t index, const char* fmt, va_list list) {
    va_list list_2;
    va_copy(list_2, list);
    int req = vsnprintf(NULL, 0, fmt, list_2);
    va_end(list_2);

    if (req >= 0) {
        uint32_t old_count = str->count;
        uint32_t new_count = old_count + (uint32_t)req;
        assert((uint64_t)old_count + (uint64_t)req <= uint32_max);
        assert(index <= old_count);

        cr_str_ensure_capacity(str, new_count + 1);

        memmove(str->data + index + (uint32_t)req, str->data + index, old_count - index);
        char c = str->data[index + (uint32_t)req];
        vsnprintf(str->data + index, (uint32_t)req + 1, fmt, list);
        str->count = new_count;
        str->data[index + (uint32_t)req] = c;
        str->data[new_count] = '\0';
    }
}

void cr_str_insert_data(struct cr_str* str, uint32_t index, const char* data, uint32_t data_count) {
    uint32_t old_count = str->count;
    uint32_t new_count = old_count + data_count;
    assert((uint64_t)old_count + (uint64_t)data_count <= uint32_max);
    assert(index <= old_count);

    uint32_t offset_in_self = uint32_max;
    if (data >= str->data && data < str->data + old_count) {
        offset_in_self = (uint32_t)(data - str->data);
    }

    cr_str_ensure_capacity(str, new_count + 1);

    if (offset_in_self != uint32_max) {
        data = str->data + offset_in_self;
    }

    memmove(str->data + index + data_count, str->data + index, old_count - index);
    memcpy(str->data + index, data, data_count);
    str->count = new_count;
    str->data[new_count] = '\0';
}

void cr_str_insert_cstr(struct cr_str* str, uint32_t index, const char* cstr) {
    cr_str_insert_data(str, index, cstr, (uint32_t)strlen(cstr));
}

void cr_str_insert(struct cr_str* str, uint32_t index, struct cr_str* str_2) {
    cr_str_insert_data(str, index, str_2->data, str_2->count);
}

void cr_str_append_data(struct cr_str* str, const char* data, uint32_t data_count) {
    cr_str_insert_data(str, str->count, data, data_count);
}

void cr_str_append_cstr(struct cr_str* str, const char* cstr) {
    cr_str_insert_data(str, str->count, cstr, (uint32_t)strlen(cstr));
}

void cr_str_append(struct cr_str* str, struct cr_str* str_2) {
    cr_str_insert_data(str, str->count, str_2->data, str_2->count);
}

void cr_str_append_format(struct cr_str* str, const char* fmt, ...) {
    struct cr_str* str_2 = cr_str_create_empty(str->mem);

    va_list list;
    va_start(list, fmt);
    cr_str_insert_va(str_2, 0, fmt, list);
    va_end(list);

    cr_str_append(str, str_2);

    cr_str_destroy(str_2);
}

void cr_str_remove_all(struct cr_str* str) {
    cr_str_ensure_capacity(str, 1);
    str->count = 0;
    str->data[0] = '\0';
}

void cr_str_remove_last(struct cr_str* str, uint32_t n) {
    uint32_t old_count = str->count;
    assert(old_count >= n);
    uint32_t new_count = old_count - n;
    cr_str_ensure_capacity(str, new_count + 1);
    str->count = new_count;
    str->data[new_count] = '\0';
}

static void _cr_str_clean_path(struct cr_str* str) {
    uint32_t count = str->count;
    if (count == 0) {
        return;
    }
    char* target = str->data;
    char current = str->data[0];
    for (uint32_t i = 0; i < count; i++) {
        char next = str->data[i + 1];
        if ((current != cr_path_div) || (next != cr_path_div)) {
            *target++ = current;
        }
        current = next;
    }
    uint32_t new_count = (uint32_t)(target - str->data);
    if ((new_count > 1) && (str->data[new_count - 1] == cr_path_div)) {
        new_count--;
    }
    cr_str_ensure_capacity(str, new_count + 1);
    str->count = new_count;
    str->data[new_count] = '\0';
}

void cr_str_append_path_component(struct cr_str* str, struct cr_str* component) {
    uint32_t old_count = str->count;
    bool add_div = (old_count > 0) && (str->data[old_count - 1] != cr_path_div);
    uint32_t new_count = old_count + add_div + component->count;
    cr_str_ensure_capacity(str, new_count + 1);
    str->count = new_count;
    char* ptr = &str->data[old_count];
    if (add_div) {
        *ptr = cr_path_div;
        ptr++;
    }
    if (component->count) {
        memcpy(ptr, component->data, component->count);
        ptr += component->count;
    }
    *ptr = '\0';
    _cr_str_clean_path(str);
}

void cr_str_remove_last_path_component(struct cr_str* str) {
    _cr_str_clean_path(str);
    if (str->count == 0) return;
    uint32_t new_count = 0;
    char* ptr = &str->data[str->count - 1];
    while (ptr >= str->data) {
        if (*ptr == cr_path_div) {
            new_count = ptr == str->data ? 1 : (uint32_t)(ptr - str->data);
            break;
        }
        ptr--;
    }
    cr_str_ensure_capacity(str, new_count + 1);
    str->count = new_count;
    str->data[new_count] = '\0';
}

struct cr_str* cr_str_get_last_path_component(struct cr_mem* mem, struct cr_str* str) {
    if (str->count < 2) {
        return cr_str_create_copy(mem, str);
    }
    char* last = &str->data[str->count - 1];
    while (last != str->data) {
        if (*last != cr_path_div) {
            break;
        }
        last--;
    }
    char* first = last;
    while (first != str->data) {
        if (*first == cr_path_div) {
            break;
        }
        first--;
    }
    if ((*first == cr_path_div) && (first < last)) {
        first++;
    }
    uint32_t count = (uint32_t)(last - first) + 1;
    struct cr_str* res = cr_str_create_empty(mem);
    cr_str_ensure_capacity(res, count + 1);
    memcpy(res->data, first, count);
    res->count = count;
    res->data[count] = '\0';
    return res;
}

bool cr_str_append_path_extension(struct cr_str* str, struct cr_str* extension) {
    uint32_t ext_count = extension->count;
    if (ext_count == 0) {
        return false;
    }
    if (extension->data[ext_count - 1] == '.') {
        return false;
    }
    char* ptr = extension->data;
    while (*ptr) {
        if (*ptr++ == cr_path_div) {
            return false;
        }
    }
    _cr_str_clean_path(str);
    uint32_t str_count = str->count;
    if (str_count == 0) {
        return false;
    }
    if ((str_count == 1) && (str->data[0] == cr_path_div)) {
        return false;
    }
    uint32_t new_count = str_count + ext_count + 1;
    cr_str_ensure_capacity(str, new_count + 1);
    str->count = new_count;
    ptr = &str->data[str_count];
    *ptr++ = '.';
    memcpy(ptr, extension->data, ext_count);
    ptr += ext_count;
    *ptr = '\0';
    return true;
}

static bool cr_str_scan_path_extension(struct cr_str* str, char* last, char** out_first) {
    char* first = last;
    char current = *first;
    while (first != str->data) {
        char next = *(first - 1);
        if (next == cr_path_div) {
            return false;
        } else if (current == '.') {
            goto has_ext;
        }
        current = next;
        first--;
    }
    return false;
has_ext: ;
    char* ptr = first;
    while (ptr != str->data) {
        if (*ptr == cr_path_div) {
            break;
        }
        ptr--;
    }
    if ((*ptr == cr_path_div) && (ptr < first)) {
        ptr++;
    }
    if (*ptr == '.') {
        return false;
    }
    *out_first = first;
    return true;
}

void cr_str_remove_path_extension(struct cr_str* str) {
    _cr_str_clean_path(str);
    if (str->count < 3) {
        return;
    }
    char* last = &str->data[str->count - 1];
    char* first;
    if (!cr_str_scan_path_extension(str, last, &first)) {
        return;
    }
    if (first != last) {
        uint32_t new_count = (uint32_t)(first - str->data);
        cr_str_ensure_capacity(str, new_count + 1);
        str->count = new_count;
        str->data[new_count] = '\0';
    }
}

struct cr_str* cr_str_get_path_extension(struct cr_mem* mem, struct cr_str* str) {
    struct cr_str* res = cr_str_create_empty(mem);
    if (str->count < 3) {
        return res;
    }
    char* last = &str->data[str->count - 1];
    while (last != str->data) {
        if (*last != cr_path_div) {
            break;
        }
        last--;
    }
    char* first;
    if (!cr_str_scan_path_extension(str, last, &first)) {
        return res;
    }
    if (first != last) {
        uint32_t count = (uint32_t)(last - first);
        cr_str_ensure_capacity(res, count + 1);
        memcpy(res->data, first + 1, count);
        res->count = count;
        res->data[count] = '\0';
    }
    return res;
}

void cr_str_append_path_component_cstr(struct cr_str* str, const char* cstr) {
    struct cr_str* component = cr_str_create(str->mem, cstr);

    cr_str_append_path_component(str, component);

    cr_str_destroy(component);
}

void cr_str_append_path_component_format(struct cr_str* str, const char* fmt, ...) {
    struct cr_str* component = cr_str_create_empty(str->mem);

    va_list list;
    va_start(list, fmt);
    cr_str_insert_va(component, 0, fmt, list);
    va_end(list);

    cr_str_append_path_component(str, component);

    cr_str_destroy(component);
}

bool cr_str_append_path_extension_cstr(struct cr_str* str, const char* cstr) {
    struct cr_str* extension = cr_str_create(str->mem, cstr);

    bool result = cr_str_append_path_extension(str, extension);

    cr_str_destroy(extension);

    return result;
}

bool cr_str_append_path_extension_format(struct cr_str* str, const char* fmt, ...) {
    struct cr_str* extension = cr_str_create_empty(str->mem);

    va_list list;
    va_start(list, fmt);
    cr_str_insert_va(extension, 0, fmt, list);
    va_end(list);

    bool result = cr_str_append_path_extension(str, extension);

    cr_str_destroy(extension);

    return result;
}


enum cr_file_type cr_str_get_file_type(struct cr_str* str) {
    struct cr_str* file = cr_str_get_last_path_component(str->mem, str);
    struct cr_str* extension = cr_str_get_path_extension(str->mem, file);

    const char* ext = cr_str_get_data(extension);
    enum cr_file_type result = cr_file_type_other;

    if (strcmp(ext, "") == 0)
        if (strcmp(cr_str_get_data(file), ".DS_Store") == 0) {
            result = cr_file_type_ds_store;
        } else {
            result = cr_file_type_none;
        }
    else if (strcmp(ext, "c") == 0)
        result = cr_file_type_c_source;
    else if (strcmp(ext, "h") == 0)
        result = cr_file_type_c_header;
    else if (strcmp(ext, "cpp") == 0)
        result = cr_file_type_cpp_source;
    else if (strcmp(ext, "hpp") == 0)
        result = cr_file_type_cpp_header;
    else if (strcmp(ext, "m") == 0)
        result = cr_file_type_objc_source;
    else if (strcmp(ext, "o") == 0)
        result = cr_file_type_o;
    else if (strcmp(ext, "a") == 0)
        result = cr_file_type_a;
    else if (strcmp(ext, "metal") == 0)
        result = cr_file_type_metal;
    else if (strcmp(ext, "air") == 0)
        result = cr_file_type_air;
    else if (strcmp(ext, "metallib") == 0)
        result = cr_file_type_metallib;
    else if (strcmp(ext, "hlsl") == 0)
        result = cr_file_type_hlsl;
    else if (strcmp(ext, "wgsl") == 0)
        result = cr_file_type_wgsl;
    else if (strcmp(ext, "plist") == 0)
        result = cr_file_type_plist;
    else if (strcmp(ext, "txt") == 0)
        result = cr_file_type_txt;
    else if (strcmp(ext, "html") == 0)
        result = cr_file_type_html;
    else if (strcmp(ext, "js") == 0)
        result = cr_file_type_js;
    else if (strcmp(ext, "wasm") == 0)
        result = cr_file_type_wasm;
    else if (strcmp(ext, "br") == 0)
        result = cr_file_type_br;
    else if (strcmp(ext, "swift") == 0)
        result = cr_file_type_swift;
    else if (strcmp(ext, "swiftmodule") == 0)
        result = cr_file_type_swiftmodule;

    cr_str_destroy(file);
    cr_str_destroy(extension);

    return result;
}

bool cr_str_is_absolute_path(struct cr_str* str) {
    return (str->count != 0) && (str->data[0] == cr_path_div);
}

bool cr_str_is_equal(struct cr_str* str, struct cr_str* str_2) {
    return (str->count == str_2->count) && (memcmp(str->data, str_2->data, str->count) == 0);
}

uint64_t cr_str_hash(struct cr_str* str) {
    uint64_t hash = 0;
    if (str->count != 0) {
        hash = cr_hash(str->data, str->count, 0);
    }
    return hash;
}

static void cr_str_ensure_capacity(struct cr_str* str, uint32_t new_capacity) {
    new_capacity = max_uint32(16, next_po2_uint32(new_capacity));

    if (str->capacity != new_capacity) {
        str->data = cr_realloc(str->mem, str->data, new_capacity, cr_loc);
        str->capacity = new_capacity;
    }
}
