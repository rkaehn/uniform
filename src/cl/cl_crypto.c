#include "cl_crypto.h"

#if defined(__aarch64__) && defined(__ARM_FEATURE_AES)

#include <arm_neon.h>

struct cl_aes_ctr_keys {
    const br_block_ctr_class* vtable;
    uint8_t rk[15][16];
    uint32_t num_rounds;
};

_Static_assert(sizeof(struct cl_aes_ctr_keys) <= sizeof(br_aes_gen_ctr_keys), "");

static uint32_t cl_aes_sub_word(uint32_t w) {
    uint8x16_t v = vreinterpretq_u8_u32(vdupq_n_u32(w));
    v = vaeseq_u8(v, vdupq_n_u8(0));
    return vgetq_lane_u32(vreinterpretq_u32_u8(v), 0);
}

static void cl_aes_ctr_init(const br_block_ctr_class** ctx, const void* key, size_t key_len) {
    struct cl_aes_ctr_keys* keys = (struct cl_aes_ctr_keys*)ctx;
    assert(key_len == 16 || key_len == 24 || key_len == 32);

    keys->vtable = cl_aes_ctr_get_vtable();

    uint32_t nk = (uint32_t)key_len / 4;
    uint32_t nr = nk + 6;
    keys->num_rounds = nr;

    static const uint8_t rcon[10] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

    uint32_t w[60];
    memcpy(w, key, key_len);
    for (uint32_t i = nk; i < 4 * (nr + 1); i++) {
        uint32_t t = w[i - 1];
        if (i % nk == 0) {
            t = cl_aes_sub_word((t >> 8) | (t << 24)) ^ rcon[i / nk - 1];
        } else if (nk > 6 && i % nk == 4) {
            t = cl_aes_sub_word(t);
        }
        w[i] = w[i - nk] ^ t;
    }
    memcpy(keys->rk, w, 16 * (nr + 1));
}

static uint32_t cl_aes_ctr_run(const br_block_ctr_class* const* ctx, const void* iv, uint32_t cc, void* data, size_t len) {
    const struct cl_aes_ctr_keys* keys = (const struct cl_aes_ctr_keys*)ctx;
    uint32_t nr = keys->num_rounds;

    uint8x16_t rk[15];
    for (uint32_t i = 0; i <= nr; i++) {
        rk[i] = vld1q_u8(keys->rk[i]);
    }

    uint8_t* buf = data;
    uint8_t ctr[16];
    memcpy(ctr, iv, 12);

    while (len >= 8 * 16) {
        uint8x16_t x[8];
        for (uint32_t b = 0; b < 8; b++) {
            uint32_t be = __builtin_bswap32(cc);
            memcpy(ctr + 12, &be, 4);
            x[b] = vld1q_u8(ctr);
            cc++;
        }
        for (uint32_t r = 0; r + 1 < nr; r++) {
            for (uint32_t b = 0; b < 8; b++) {
                x[b] = vaesmcq_u8(vaeseq_u8(x[b], rk[r]));
            }
        }
        for (uint32_t b = 0; b < 8; b++) {
            x[b] = veorq_u8(vaeseq_u8(x[b], rk[nr - 1]), rk[nr]);
        }
        for (uint32_t b = 0; b < 8; b++) {
            vst1q_u8(buf + 16 * b, veorq_u8(vld1q_u8(buf + 16 * b), x[b]));
        }
        buf += 8 * 16;
        len -= 8 * 16;
    }

    while (len > 0) {
        uint32_t be = __builtin_bswap32(cc);
        memcpy(ctr + 12, &be, 4);
        uint8x16_t x = vld1q_u8(ctr);
        for (uint32_t r = 0; r + 1 < nr; r++) {
            x = vaesmcq_u8(vaeseq_u8(x, rk[r]));
        }
        x = veorq_u8(vaeseq_u8(x, rk[nr - 1]), rk[nr]);
        if (len >= 16) {
            cc++;
            vst1q_u8(buf, veorq_u8(vld1q_u8(buf), x));
            buf += 16;
            len -= 16;
        } else {
            uint8_t tmp[16];
            vst1q_u8(tmp, x);
            for (size_t i = 0; i < len; i++) {
                buf[i] ^= tmp[i];
            }
            len = 0;
        }
    }

    return cc;
}

static const br_block_ctr_class cl_aes_ctr_vtable = {
    .context_size = sizeof(struct cl_aes_ctr_keys),
    .block_size = 16,
    .log_block_size = 4,
    .init = cl_aes_ctr_init,
    .run = cl_aes_ctr_run,
};

const br_block_ctr_class* cl_aes_ctr_get_vtable(void) {
    return &cl_aes_ctr_vtable;
}

struct cl_ghash_prod {
    uint8x16_t lo;
    uint8x16_t hi;
};

static struct cl_ghash_prod cl_ghash_mul256(uint8x16_t a, uint8x16_t b) {
    poly64_t a0 = vgetq_lane_p64(vreinterpretq_p64_u8(a), 0);
    poly64_t a1 = vgetq_lane_p64(vreinterpretq_p64_u8(a), 1);
    poly64_t b0 = vgetq_lane_p64(vreinterpretq_p64_u8(b), 0);
    poly64_t b1 = vgetq_lane_p64(vreinterpretq_p64_u8(b), 1);

    uint8x16_t r0 = vreinterpretq_u8_p128(vmull_p64(a0, b0));
    uint8x16_t r1 = vreinterpretq_u8_p128(vmull_p64(a1, b1));
    uint8x16_t m = veorq_u8(vreinterpretq_u8_p128(vmull_p64(a0, b1)), vreinterpretq_u8_p128(vmull_p64(a1, b0)));

    uint8x16_t zero = vdupq_n_u8(0);
    return (struct cl_ghash_prod) {
        .lo = veorq_u8(r0, vextq_u8(zero, m, 8)),
        .hi = veorq_u8(r1, vextq_u8(m, zero, 8)),
    };
}

static struct cl_ghash_prod cl_ghash_prod_xor(struct cl_ghash_prod a, struct cl_ghash_prod b) {
    return (struct cl_ghash_prod) {
        .lo = veorq_u8(a.lo, b.lo),
        .hi = veorq_u8(a.hi, b.hi),
    };
}

static uint8x16_t cl_ghash_reduce(struct cl_ghash_prod p) {
    uint8x16_t zero = vdupq_n_u8(0);
    const poly64_t c = 0x87;
    uint8x16_t f0 = vreinterpretq_u8_p128(vmull_p64(vgetq_lane_p64(vreinterpretq_p64_u8(p.hi), 0), c));
    uint8x16_t f1 = vreinterpretq_u8_p128(vmull_p64(vgetq_lane_p64(vreinterpretq_p64_u8(p.hi), 1), c));

    uint8x16_t r = veorq_u8(p.lo, f0);
    r = veorq_u8(r, vextq_u8(zero, f1, 8));
    r = veorq_u8(r, vreinterpretq_u8_p128(vmull_p64(vgetq_lane_p64(vreinterpretq_p64_u8(f1), 1), c)));
    return r;
}

static uint8x16_t cl_ghash_mul(uint8x16_t a, uint8x16_t b) {
    return cl_ghash_reduce(cl_ghash_mul256(a, b));
}

static void cl_ghash_pmull(void* y, const void* h, const void* data, size_t len) {
    const uint8_t* buf = data;
    uint8x16_t yv = vrbitq_u8(vld1q_u8(y));
    uint8x16_t hv = vrbitq_u8(vld1q_u8(h));

    if (len >= 8 * 16) {
        uint8x16_t hp[8];
        hp[0] = hv;
        for (uint32_t i = 1; i < 8; i++) {
            hp[i] = cl_ghash_mul(hp[i - 1], hv);
        }
        while (len >= 8 * 16) {
            struct cl_ghash_prod p = cl_ghash_mul256(veorq_u8(yv, vrbitq_u8(vld1q_u8(buf))), hp[7]);
            for (uint32_t i = 1; i < 8; i++) {
                p = cl_ghash_prod_xor(p, cl_ghash_mul256(vrbitq_u8(vld1q_u8(buf + 16 * i)), hp[7 - i]));
            }
            yv = cl_ghash_reduce(p);
            buf += 8 * 16;
            len -= 8 * 16;
        }
    }

    while (len > 0) {
        uint8x16_t x;
        if (len >= 16) {
            x = vld1q_u8(buf);
            buf += 16;
            len -= 16;
        } else {
            uint8_t tmp[16] = { 0 };
            memcpy(tmp, buf, len);
            x = vld1q_u8(tmp);
            len = 0;
        }
        yv = cl_ghash_mul(veorq_u8(yv, vrbitq_u8(x)), hv);
    }

    vst1q_u8(y, vrbitq_u8(yv));
}

br_ghash cl_ghash_get(void) {
    return &cl_ghash_pmull;
}

#else

const br_block_ctr_class* cl_aes_ctr_get_vtable(void) {
    return br_aes_x86ni_ctr_get_vtable();
}

br_ghash cl_ghash_get(void) {
    return br_ghash_pclmul_get();
}

#endif

void cl_crypto_init_engine(br_ssl_engine_context* eng) {
    const br_block_ctr_class* aes = cl_aes_ctr_get_vtable();
    br_ghash gh = cl_ghash_get();
    if (aes == NULL || gh == NULL) {
        return;
    }

    br_ssl_engine_set_aes_ctr(eng, aes);
    br_ssl_engine_set_ghash(eng, gh);

    static const uint16_t suites[] = {
        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
        BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        BR_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        BR_TLS_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_RSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_RSA_WITH_AES_128_CBC_SHA256,
        BR_TLS_RSA_WITH_AES_256_CBC_SHA256,
        BR_TLS_RSA_WITH_AES_128_CBC_SHA,
        BR_TLS_RSA_WITH_AES_256_CBC_SHA,
    };
    br_ssl_engine_set_suites(eng, suites, sizeof(suites) / sizeof(suites[0]));
    br_ssl_engine_add_flags(eng, BR_OPT_ENFORCE_SERVER_PREFERENCES);
}
