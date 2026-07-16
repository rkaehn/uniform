#pragma once

#include "cr.h"

struct app;

#if cr_macos || cr_wasm
bool app_test_core_(struct app* app);
#endif

#if cr_macos
bool app_test_window_(struct app* app);
#endif
