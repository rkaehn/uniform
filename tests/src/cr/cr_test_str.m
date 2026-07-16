#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "cr.h"
#include "cr_tests.h"

#if cr_ios || cr_macos
#import <Foundation/Foundation.h>

#define cr_test_path_len 4

static bool make_path(char* path, int* c, int* l) {
    char set[] = { '.', '/', 'a' };
    c[0]++;
    for (int i = 0; i < *l - 1; i++) {
        if (c[i] == sizeof(set)) {
            c[i] = 0;
            c[i + 1]++;
        }
    }
    if (c[*l - 1] == sizeof(set)) {
        if (*l == cr_test_path_len) return false;
        c[*l - 1] = 0;
        *l += 1;
    }
    for (int i = 0; i < *l; i++) {
        path[i] = set[c[i]];
    }
    path[*l] = '\0';
    return true;
}

static void reset_path(int* c, int* l) {
    c[0] = -1;
    for (int i = 1; i < cr_test_path_len; i++) {
        c[i] = 0;
    }
    *l = 1;
}

static void test_str_paths(struct cr_mem* mem) {
    int saved_stderr = dup(STDERR_FILENO);
    freopen("/dev/null", "w", stderr);

    char path1[cr_test_path_len + 1] = { 0 };
    char path2[cr_test_path_len + 1] = { 0 };
    int c1[cr_test_path_len];
    int c2[cr_test_path_len];
    int l1;
    int l2;

    reset_path(c1, &l1);
    while (make_path(path1, c1, &l1)) {
        struct cr_str* input = cr_str_create(mem, path1);
        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
        reset_path(c2, &l2);
        while (make_path(path2, c2, &l2)) {
            struct cr_str* output = cr_str_create_copy(mem, input);
            struct cr_str* path_component = cr_str_create(mem, path2);
            cr_str_append_path_component(output, path_component);
            NSString* append_ns = [NSString stringWithCString:path2 encoding:NSUTF8StringEncoding];
            NSString* output_ns = [input_ns stringByAppendingPathComponent:append_ns];
            size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
            assert(cr_str_get_count(output) == count);
            assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
            cr_str_destroy(output);
            cr_str_destroy(path_component);
        }
        cr_str_destroy(input);
    }

    reset_path(c1, &l1);
    while (make_path(path1, c1, &l1)) {
        struct cr_str* input = cr_str_create(mem, path1);
        struct cr_str* output = cr_str_create_copy(mem, input);
        cr_str_remove_last_path_component(output);
        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
        NSString* output_ns = [input_ns stringByDeletingLastPathComponent];
        size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        assert(cr_str_get_count(output) == count);
        assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
        cr_str_destroy(input);
        cr_str_destroy(output);
    }

    reset_path(c1, &l1);
    while (make_path(path1, c1, &l1)) {
        struct cr_str* input = cr_str_create(mem, path1);
        struct cr_str* output = cr_str_get_last_path_component(mem, input);
        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
        NSString* output_ns = [input_ns lastPathComponent];
        size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        assert(cr_str_get_count(output) == count);
        assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
        cr_str_destroy(input);
        cr_str_destroy(output);
    }

    reset_path(c1, &l1);
    while (make_path(path1, c1, &l1)) {
        struct cr_str* input = cr_str_create(mem, path1);
        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
        reset_path(c2, &l2);
        while (make_path(path2, c2, &l2)) {
            struct cr_str* output = cr_str_create_copy(mem, input);
            struct cr_str* path_extension = cr_str_create(mem, path2);
            bool success = cr_str_append_path_extension(output, path_extension);
            NSString* append_ns = [NSString stringWithCString:path2 encoding:NSUTF8StringEncoding];
            NSString* output_ns = [input_ns stringByAppendingPathExtension:append_ns];
            if (output_ns) {
                size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
                assert(success);
                assert(cr_str_get_count(output) == count);
                assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
            } else {
                assert(!success);
            }
            cr_str_destroy(output);
            cr_str_destroy(path_extension);
        }
        cr_str_destroy(input);
    }

//    reset_path(c1, &l1);
//    while (make_path(path1, c1, &l1)) {
//        struct cr_str* input = cr_str_create(mem, path1);
//        struct cr_str* output = cr_str_create_copy(mem, input);
//        cr_str_remove_path_extension(output);
//        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
//        NSString* output_ns = [input_ns stringByDeletingPathExtension];
//        size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
//        assert(cr_str_get_count(output) == count);
//        assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
//        cr_str_destroy(input);
//        cr_str_destroy(output);
//    }
//
//    reset_path(c1, &l1);
//    while (make_path(path1, c1, &l1)) {
//        struct cr_str* input = cr_str_create(mem, path1);
//        struct cr_str* output = cr_str_get_path_extension(mem, input);
//        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
//        NSString* output_ns = [input_ns pathExtension];
//        size_t count = [output_ns lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
//        assert(cr_str_get_count(output) == count);
//        assert(memcmp(cr_str_get_data(output), [output_ns cStringUsingEncoding:NSUTF8StringEncoding], count) == 0);
//        cr_str_destroy(input);
//        cr_str_destroy(output);
//    }

    reset_path(c1, &l1);
    while (make_path(path1, c1, &l1)) {
        struct cr_str* input = cr_str_create(mem, path1);
        bool result = cr_str_is_absolute_path(input);
        NSString* input_ns = [NSString stringWithCString:path1 encoding:NSUTF8StringEncoding];
        bool result_ns = [input_ns isAbsolutePath];
        assert(result == result_ns);
        cr_str_destroy(input);
    }

    fflush(stderr);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
}
#endif

void cr_test_str(void) {
    struct cr_mem* mem = cr_mem_libc;

#if cr_ios || cr_macos
    test_str_paths(mem);
#endif

#if !cr_wasm && !cr_ios
    {
        struct cr_str* empty = cr_str_create_empty(mem);
        struct cr_str* path = cr_str_create(mem, "/tmp/cr_test_empty_file");
        bool wrote = cr_str_write_to_file(empty, path);
        assert(wrote);
        struct cr_str* read_back = cr_str_create_with_contents_of_file(mem, path);
        assert(read_back != NULL);
        assert(cr_str_get_count(read_back) == 0);
        cr_util_remove(path);
        cr_str_destroy(read_back);
        cr_str_destroy(path);
        cr_str_destroy(empty);
    }
#endif

    printf("cr_test_str: ok\n");
}
