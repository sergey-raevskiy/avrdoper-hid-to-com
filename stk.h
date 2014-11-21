#pragma once

#include <stdint.h>
#include <stdio.h>
#include "runtime.h"

#define STK_MAGIC 0x1b
#define STK_TOKEN 0x0e

enum {
    stk_msg_magic = 0,
    stk_msg_seq,
    stk_msg_size_hi,
    stk_msg_size_lo,
    stk_msg_token,

    stk_msg_header_len
};

err_t * stk_read_message(unsigned char **message_p, serial_t *serial,
                         int timeout, pool_t *pool);
err_t * stk_write_message(serial_t *serial, const unsigned char *message,
                          pool_t *pool);

void stk_dump_message(FILE *out, const unsigned char *message);
