#pragma once

#include <windows.h>

// Pools

typedef struct pool_t pool_t;
typedef void (* pool_cleanup_t)(void *data);

pool_t * pool_create(pool_t *parent);
void pool_add_cleanup(pool_t *pool, pool_cleanup_t cleanup, void *data);
void * pool_calloc(pool_t *pool, size_t size);
void pool_clear(pool_t *pool);
void pool_destroy(pool_t *pool);

// Errors

typedef struct err_t {
    pool_t *pool;
    DWORD code;
    const wchar_t *msg;
} err_t;

err_t * err_create(DWORD code, const wchar_t *msg);
void err_clear(err_t *err);
const wchar_t * err_str(err_t *err, pool_t *pool);

#define ERR(expr) do {         \
    err_t *__err = (expr);     \
    if (__err) return __err;   \
} while (0)

// Serial

typedef int (* serial_enumerate_callback_t)(void *data,
                                            const wchar_t *id,
                                            pool_t *pool);

typedef struct serial_private serial_private_t;

typedef struct {
    err_t * (* enumerate)(serial_enumerate_callback_t callback, void *data,
                          pool_t *pool);
    err_t * (* open)(serial_private_t **priv, const wchar_t *id, pool_t *pool);
    err_t * (* read)(serial_private_t *priv, unsigned char *buffer, size_t *len,
                     pool_t *pool);
    err_t * (* write)(serial_private_t *priv, const unsigned char *data, size_t len,
                      pool_t *pool);
} serial_vtable_t;

extern const serial_vtable_t com;
extern const serial_vtable_t avrdoper_hid;

typedef struct serial serial_t;

err_t * serial_enum(const serial_vtable_t *vtable,
                    serial_enumerate_callback_t callback,
                    void *data,
                    pool_t *pool);

err_t * serial_open(serial_t **serial,
                    const serial_vtable_t *vtable,
                    const wchar_t *id, 
                    pool_t *pool);

err_t * serial_read(serial_t *serial, unsigned char *buffer, size_t *len,
                    pool_t *pool);

err_t * serial_write(serial_t *serial, const unsigned char *data, size_t len,
                     pool_t *pool);

err_t * serial_read_full(serial_t *serial, unsigned char *buffer, size_t len,
                         int timeout, pool_t *pool);
