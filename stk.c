#include "stk.h"

unsigned char checksum(unsigned char *data, size_t len) {
    unsigned char sum = 0;
    while (len--)
        sum ^= *data++;
    return sum;
}

size_t msg_full_len(const unsigned char *msg) {
    size_t len = 0;
    len += stk_msg_header_len;
    len += (msg[stk_msg_size_hi] << 8) + msg[stk_msg_size_lo];
    len += 1;
    return len;
}

err_t * stk_read_message(unsigned char **message_p, serial_t *serial,
                         int timeout, pool_t *pool) {
    unsigned char header[stk_msg_header_len];
    unsigned char *message;
    size_t nalloc;

    ERR(serial_read_full(serial, header, sizeof(header), timeout, pool));

    if (header[stk_msg_magic] != STK_MAGIC)
        return err_create(ERROR_INVALID_DATA, L"Invalid message magic");

    if (header[stk_msg_token] != STK_TOKEN)
        return err_create(ERROR_INVALID_DATA, L"Unexpected message token");

    nalloc = msg_full_len(header);

    message = pool_calloc(pool, nalloc);
    memcpy(message, header, sizeof(header));

    ERR(serial_read_full(serial, (message + stk_msg_header_len),
                         nalloc - stk_msg_header_len, timeout, pool));

    if (checksum(message, nalloc - 1) != message[nalloc - 1]) {
        return err_create(ERROR_INVALID_DATA, L"Invalid checksum");
    }

    *message_p = message;
    return NULL;
}

err_t * stk_write_message(serial_t *serial, const unsigned char *message,
                          pool_t *pool) {
    size_t len = msg_full_len(message);
    return serial_write(serial, message, len, pool);
}
