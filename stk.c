#include "stk.h"

unsigned char checksum(const unsigned char *data, size_t len) {
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

static void hexdump(FILE *out, const unsigned char *data, size_t len) {
    while (len--)
        fwprintf(out, L"%02X ", (int) *data++);
    fwprintf(out, L"\n");
}

void stk_dump_message(FILE *out, const unsigned char *message) {
    int body_sz = (message[stk_msg_size_hi] << 8) + message[stk_msg_size_lo];

    fwprintf(out, L"magic %s, sequence %d, token 0x%02X, size %d, body:\n",
             message[stk_msg_magic] == STK_MAGIC ? L"valid" : L"invalid",
             (int) message[stk_msg_seq],
             (int) message[stk_msg_token],
             body_sz);

    hexdump(out, message + stk_msg_header_len, body_sz);
    if (message[msg_full_len(message) - 1] ==
        checksum(message, msg_full_len(message) - 1)) {
        fwprintf(out, L"checksum 0x%02X (valid)\n",
                 (int) message[msg_full_len(message) - 1]);
    } else {
        fwprintf(out, L"checksum mismatch expected 0x%02X, got 0x%02X\n",
                 checksum(message, msg_full_len(message) - 1),
                 (int) message[msg_full_len(message) - 1]);
    }
}
