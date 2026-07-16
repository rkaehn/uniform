#pragma once

#include <sys/resource.h>
#include "cl.h"

#define cl_log(...) printf(__VA_ARGS__)
#define cl_errno strerror(errno)

struct cl_decode_pem_context {
    uint8_t* output;
    uint8_t* output_end;
};

cr_hdef void cl_decoder_dest(void* args, const void* data, size_t size) {
    struct cl_decode_pem_context* context = args;
    if (context->output + size <= context->output_end) {
        memcpy(context->output, data, size);
    }
    context->output += size;
}

cr_hdef ssize_t cl_decode_pem(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size, void** obj_data, size_t* obj_size, size_t* obj_count) {
    ssize_t result = -1;

    br_pem_decoder_context dc;
    br_pem_decoder_init(&dc);

    size_t out_obj_count = 0;
    size_t read = 0;
    uint8_t* last_output = output;
    struct cl_decode_pem_context context = {
        .output = output,
        .output_end = output + output_size,
    };

    br_pem_decoder_setdest(&dc, cl_decoder_dest, &context);

    while (read < input_size) {
        read += br_pem_decoder_push(&dc, input + read, input_size - read);

        if (context.output > context.output_end) {
            cl_log("error while decoding (buf)\n");
            goto end;
        }

        int status = br_pem_decoder_event(&dc);

        switch (status) {
        case BR_PEM_BEGIN_OBJ:
            if (out_obj_count == *obj_count) {
                cl_log("error while decoding (count)\n");
                goto end;
            }
            break;
        case BR_PEM_END_OBJ:
            obj_data[out_obj_count] = last_output;
            obj_size[out_obj_count] = (size_t)(context.output - last_output);
            last_output = context.output;
            out_obj_count++;
            break;
        case BR_PEM_ERROR:
            cl_log("error while decoding certificates (format)\n");
            goto end;
        }
    }

    *obj_count = out_obj_count;
    result = context.output - output;

end:
    return result;

}

cr_hdef int cl_header_is(const struct phr_header* header, const char* name) {
    size_t name_size = strlen(name);
    return header->name_len == name_size && strncasecmp(header->name, name, name_size) == 0;
}

cr_hdef int cl_inc_fd_limit(void) {
    struct rlimit limit;
    int err = getrlimit(RLIMIT_NOFILE, &limit);
    if (err == -1) {
        cl_log("getrlimit(): %s\n", cl_errno);
        return -1;
    }

    limit.rlim_cur = limit.rlim_max;

    err = setrlimit(RLIMIT_NOFILE, &limit);
    if (err == -1) {
        cl_log("setrlimit(): %s\n", cl_errno);
        return -1;
    }

    return 0;
}
