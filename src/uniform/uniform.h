#pragma once

#include "cr.h"

int uniform_run(const char* script_path, int argc, char** argv, int script_args);
int uniform_serve(const char* script_path, int port, const char* bind_address, const char* device, int argc, char** argv, int script_args);
int uniform_device(const char* script_path, const char* device, int argc, char** argv, int script_args);
struct cr_str* uniform_root(struct cr_mem* mem);
int uniform_certificate(const char* device);
