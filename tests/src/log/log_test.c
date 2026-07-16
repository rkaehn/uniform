#include "log.h"
#include "log_tests.h"
#include "cr.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define test_log_glyph_capacity (1u << 15)

struct test_log_glyph {
    int32_t row;
    uint32_t col;
    uint32_t cp;
};

struct test_log_state {
    struct test_log_glyph exp[test_log_glyph_capacity];
    uint32_t exp_count;
    struct test_log_glyph got[test_log_glyph_capacity];
    uint32_t got_count;
    uint32_t exp_row_count;
};

static void test_log_expect(struct test_log_state* t, const char* const* msgs, const uint32_t* sizes, uint32_t msg_count, uint32_t width) {
    t->exp_count = 0;

    uint32_t u = 0;
    uint32_t v = 0;

    for (uint32_t m = 0; m < msg_count; m++) {
        bool wants_new_row = m != 0;

        for (uint32_t i = 0; i < sizes[m]; i++) {
            uint32_t cp = (uint32_t)(uint8_t)msgs[m][i];

            bool is_overflow = wants_new_row == false && u == width;
            uint32_t next_u = u;
            uint32_t next_v = v;

            if (wants_new_row || is_overflow) {
                wants_new_row = false;
                next_u = 0;
                next_v++;
            }

            assert(t->exp_count < test_log_glyph_capacity);
            t->exp[t->exp_count++] = (struct test_log_glyph) { .row = (int32_t)next_v, .col = next_u, .cp = cp };

            u = next_u;
            v = next_v;

            if (cp == '\n') {
                if (is_overflow == false) {
                    u = 0;
                    v++;
                }
            } else {
                u++;
            }
        }
    }

    t->exp_row_count = v + 1;

    for (uint32_t i = 0; i < t->exp_count; i++) {
        t->exp[i].row -= (int32_t)t->exp_row_count;
    }
}

static void test_log_collect(int32_t row, uint32_t col, uint32_t cp, void* user) {
    struct test_log_state* t = user;

    assert(t->got_count < test_log_glyph_capacity);
    t->got[t->got_count++] = (struct test_log_glyph) { .row = row, .col = col, .cp = cp };
}

static void test_log_check_range(struct test_log_state* t, uint32_t width, int32_t row_start, int32_t row_end) {
    t->got_count = 0;
    log_view_glyphs(width, row_start, row_end, test_log_collect, t);

    uint32_t k = 0;

    for (uint32_t i = 0; i < t->exp_count; i++) {
        if (t->exp[i].row < row_start || t->exp[i].row >= row_end) {
            continue;
        }
        assert(k < t->got_count);
        assert(t->got[k].row == t->exp[i].row);
        assert(t->got[k].col == t->exp[i].col);
        assert(t->got[k].cp == t->exp[i].cp);
        k++;
    }

    assert(k == t->got_count);
}

static void test_log_check_full(struct test_log_state* t, uint32_t width) {
    assert(log_view_row_count(width) == t->exp_row_count);

    test_log_check_range(t, width, -(int32_t)t->exp_row_count, 0);
    assert(t->got_count == t->exp_count);
}

void log_test(void) {
    int err = log_init(NULL);
    assert(err == 0);
    assert(log_is_active());

    struct test_log_state* t = malloc(sizeof(*t));
    memset(t, 0, sizeof(*t));

    {
        log_s("ab\ncd");

        const char* msgs[] = { "ab\ncd" };
        uint32_t sizes[] = { 5 };
        test_log_expect(t, msgs, sizes, 1, 8);

        assert(t->exp_row_count == 2);
        test_log_check_full(t, 8);

        log_clear();
        assert(log_view_row_count(8) == 0);

        t->got_count = 0;
        log_view_glyphs(8, -8, 0, test_log_collect, t);
        assert(t->got_count == 0);
    }

    {
        log_s("h\xc3\xa9llo");

        assert(log_view_row_count(16) == 1);

        t->got_count = 0;
        log_view_glyphs(16, -1, 0, test_log_collect, t);
        assert(t->got_count == 5);
        assert(t->got[0].cp == 'h' && t->got[0].col == 0);
        assert(t->got[1].cp == 0xe9 && t->got[1].col == 1);
        assert(t->got[4].cp == 'o' && t->got[4].col == 4);

        log_clear();
    }

    {
        log_s("frozen");
        log_pause();
        assert(log_is_paused());

        log_s("after freeze");
        assert(log_view_row_count(16) == 1);

        log_resume();
        assert(log_is_paused() == false);
        assert(log_view_row_count(16) == 2);

        log_clear();
    }

    {
        static char msgs[4][1025];
        const char* msg_ptrs[4];
        uint32_t sizes[4];

        uint64_t seed = 0;

        for (uint32_t m = 0; m < 4; m++) {
            uint32_t size = 1u + (uint32_t)(cr_rand(&seed) & 1023u);

            for (uint32_t i = 0; i < size; i++) {
                uint64_t r = cr_rand(&seed);
                msgs[m][i] = (r & 0x0f) != 0 ? (char)('a' + ((r >> 4) & 7)) : '\n';
            }
            msgs[m][size] = 0;

            msg_ptrs[m] = msgs[m];
            sizes[m] = size;

            log_s("%s", msgs[m]);
        }

        static const uint32_t widths[] = { 3, 8, 40 };

        for (uint32_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
            uint32_t width = widths[w];

            test_log_expect(t, msg_ptrs, sizes, 4, width);
            test_log_check_full(t, width);

            int32_t count = (int32_t)t->exp_row_count;
            test_log_check_range(t, width, -count + 2, -count + 5);
            test_log_check_range(t, width, -3, 0);
            test_log_check_range(t, width, -count - 8, -count + 1);
            test_log_check_range(t, width, 5, 10);
        }

        log_clear();
    }

    {
        for (uint32_t i = 0; i < 400; i++) {
            log_s("wrap line %u with some padding text to fill the ring buffer", i);
        }

        uint32_t rows = log_view_row_count(8);
        assert(rows > 0);

        t->got_count = 0;
        log_view_glyphs(8, -(int32_t)rows, 0, test_log_collect, t);
        assert(t->got_count > 0);

        for (uint32_t i = 0; i < t->got_count; i++) {
            assert(t->got[i].row >= -(int32_t)rows && t->got[i].row < 0);
            assert(t->got[i].col < 8u);
        }

        log_clear();
    }

#if !defined(__wasm__)
    {
        log_set_mode(log_mode_term);

        printf("captured one\ncaptured two\n");
        log_dump();
        assert(log_view_row_count(40) == 2);

        printf("captured three\n");
        log_dump();
        assert(log_view_row_count(40) == 3);

        log_set_mode(log_mode_tool);
        log_clear();
    }
#endif

    free(t);

    log_deinit();
    assert(log_is_active() == false);

    printf("log_test: ok\n");
}
