#pragma once

#include "cr.h"
cr_warning_push
cr_warning_ignore_everything
#include "bearssl.h"
cr_warning_pop

const br_block_ctr_class* cl_aes_ctr_get_vtable(void);
br_ghash cl_ghash_get(void);
void cl_crypto_init_engine(br_ssl_engine_context* eng);
