#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cr.h"
#include "cr_tests.h"

void cr_test_util(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint8_t known[] = { 159, 134, 208, 129, 136, 76, 125, 101, 154, 47, 234, 160, 197, 90, 208, 21, 163, 191, 79, 27, 43, 11, 130, 44, 209, 93, 108, 21, 176, 240, 10, 8, };
    uint8_t raw[32] = { 0 };

    struct cr_sha256 sha256;
    cr_sha256_init(&sha256);
    cr_sha256_update(&sha256, "t", 1);
    cr_sha256_update(&sha256, "es", 2);
    cr_sha256_update(&sha256, "t", 1);
    cr_sha256_final(&sha256, raw);
    assert(memcmp(raw, known, 32) == 0);

#if !cr_wasm && !cr_ios
    {
        struct cr_str* arg = cr_str_create(mem, "/usr/bin/true");
        int status = cr_exec(&arg, 1, NULL, NULL);
        assert(status == 0);
        cr_str_destroy(arg);
    }

    {
        struct cr_str* arg = cr_str_create(mem, "/usr/bin/false");
        int status = cr_exec(&arg, 1, NULL, NULL);
        assert(status == 1);
        cr_str_destroy(arg);
    }

    {
        struct cr_str* args[2] = { cr_str_create(mem, "/bin/echo"), cr_str_create(mem, "uniform") };
        struct cr_str* output = cr_str_create_empty(mem);
        int status = cr_exec(args, 2, output, output);
        assert(status == 0);
        assert(strcmp(cr_str_get_data(output), "uniform\n") == 0);
        cr_str_destroy(output);
        cr_str_destroy(args[0]);
        cr_str_destroy(args[1]);
    }

    {
        struct cr_str* arg = cr_str_create(mem, "/nonexistent_binary_for_cr_exec_test");
        int status = cr_exec(&arg, 1, NULL, NULL);
        assert(status == -1);
        cr_str_destroy(arg);
    }

    {
        int fds[2];
        int err = pipe(fds);
        assert(err == 0);
        struct cr_str* args[3] = { cr_str_create(mem, "/bin/sh"), cr_str_create(mem, "-c"), cr_str_create_format(mem, "echo leak >&%d", fds[1]) };
        struct cr_str* output = cr_str_create_empty(mem);
        int status = cr_exec(args, 3, output, output);
        assert(status != 0);
        close(fds[0]);
        close(fds[1]);
        cr_str_destroy(output);
        cr_str_destroy(args[0]);
        cr_str_destroy(args[1]);
        cr_str_destroy(args[2]);
    }

    {
        struct cr_str* root = cr_str_create(mem, "/tmp/cr_test_util_dir");
        struct cr_str* nested = cr_str_create(mem, "/tmp/cr_test_util_dir/a/b");
        struct cr_str* one = cr_str_create(mem, "/tmp/cr_test_util_dir/a/one.txt");
        struct cr_str* two = cr_str_create(mem, "/tmp/cr_test_util_dir/a/b/two.txt");
        struct cr_str* copy = cr_str_create(mem, "/tmp/cr_test_util_dir_copy");
        struct cr_str* copied = cr_str_create(mem, "/tmp/cr_test_util_dir_copy/a/b/two.txt");
        struct cr_str* text = cr_str_create(mem, "hello");
        cr_util_remove(root);
        cr_util_remove(copy);
        bool created = cr_util_create_dir(nested);
        assert(created);
        bool wrote = cr_str_write_to_file(text, one) && cr_str_write_to_file(text, two);
        assert(wrote);

        uint32_t seen = 0;
        bool saw_b = false;
        struct cr_util_dir_iter iter = cr_util_dir_iter(mem, root);
        while (cr_util_dir_iter_next(&iter)) {
            const char* relative = cr_str_get_data(iter.relative_path);
            if (strcmp(relative, "a") == 0) {
                assert(seen == 0 && iter.depth == 0 && iter.file_type == cr_util_file_type_directory);
            } else if (strcmp(relative, "a/one.txt") == 0) {
                assert(iter.depth == 1 && iter.file_type == cr_util_file_type_regular && iter.file_size == 5);
            } else if (strcmp(relative, "a/b") == 0) {
                assert(iter.depth == 1 && iter.file_type == cr_util_file_type_directory);
                saw_b = true;
            } else {
                assert(strcmp(relative, "a/b/two.txt") == 0 && saw_b && iter.depth == 2 && iter.file_size == 5);
            }
            seen++;
        }
        assert(seen == 4);

        iter = cr_util_dir_iter(mem, root);
        seen = 0;
        while (cr_util_dir_iter_next(&iter)) {
            cr_util_dir_iter_skip(&iter);
            seen++;
        }
        assert(seen == 1);

        bool did_copy = cr_util_copy(mem, root, copy);
        assert(did_copy);
        struct cr_str* read_back = cr_str_create_with_contents_of_file(mem, copied);
        assert(read_back != NULL && cr_str_is_equal(read_back, text));

        cr_util_remove(root);
        cr_util_remove(copy);
        cr_str_destroy(read_back);
        cr_str_destroy(text);
        cr_str_destroy(copied);
        cr_str_destroy(copy);
        cr_str_destroy(two);
        cr_str_destroy(one);
        cr_str_destroy(nested);
        cr_str_destroy(root);
    }
#endif

    printf("cr_test_util: ok\n");
}
