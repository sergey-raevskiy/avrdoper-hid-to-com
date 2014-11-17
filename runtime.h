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
