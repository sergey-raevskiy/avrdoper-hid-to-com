#include <malloc.h>
#include <stddef.h>
#include <string.h>

#include "runtime.h"

// Pools

typedef struct cleanup_t
{
    struct cleanup_t *prev;

    pool_cleanup_t cleanup;
    void *data;
} cleanup_t;

struct pool_t
{
    pool_t *prev_sibling;
    pool_t *parent;
    pool_t *child;
    cleanup_t *cleanup;
};

pool_t * pool_create(pool_t *parent)
{
    pool_t *pool = malloc(sizeof(*pool));

    pool->child = NULL;
    pool->cleanup = NULL;
    pool->prev_sibling = NULL;
    pool->parent = parent;

    if (parent) {
        pool->prev_sibling = parent->child;
        parent->child = pool;
    }

    return pool;
}

void pool_add_cleanup(pool_t *pool, pool_cleanup_t cleanup, void *data)
{
    cleanup_t *cl = malloc(sizeof(*cl));

    cl->prev = pool->cleanup;
    cl->cleanup = cleanup;
    cl->data = data;

    pool->cleanup = cl;
}

void * pool_calloc(pool_t *pool, size_t size)
{
    void *alloc = malloc(size);

    memset(alloc, 0, size);
    pool_add_cleanup(pool, free, alloc);

    return alloc;
}

void * pool_memdup(pool_t *pool, void * mem, size_t size)
{
    void *alloc = pool_calloc(pool, size);
    memcpy(alloc, mem, size);

    return alloc;
}

wchar_t * pool_wstrcat(pool_t *pool, ...)
{
    va_list va;
    const wchar_t *str;
    wchar_t *result;
    size_t total_len = 1;

    va_start(va, pool);
    while (str = va_arg(va, const wchar_t *))
        total_len += wcslen(str);
    va_end(va);

    result = pool_calloc(pool, total_len * sizeof(wchar_t));
    va_start(va, pool);
    while (str = va_arg(va, const wchar_t *))
        wcscat_s(result, total_len, str);
    va_end(va);

    return result;
}

wchar_t *pool_wsprintf(pool_t *pool, const wchar_t *fmt, ...)
{
    va_list va;
    const wchar_t *str;
    wchar_t *result;
    size_t total_len;

    va_start(va, fmt);
    total_len = _vscwprintf(fmt, va) + 1;
    va_end(va);

    result = pool_calloc(pool, total_len * sizeof(wchar_t));
    va_start(va, fmt);
    vswprintf_s(result, total_len, fmt, va);
    va_end(va);

    return result;
}

void pool_clear(pool_t *pool)
{
    while (pool->child)
    {
        // This will remove child pool from child list.
        pool_destroy(pool->child);
    }

    while (pool->cleanup)
    {
        cleanup_t *cleanup = pool->cleanup;
        pool->cleanup = pool->cleanup->prev;

        cleanup->cleanup(cleanup->data);
        free(cleanup);
    }
}

void pool_destroy(pool_t *pool)
{
    if (pool->parent) {
        pool_t *p = pool->parent->child;

        if (p == pool) {
            pool->parent->child = pool->prev_sibling;
        } else {
            while (p->prev_sibling != pool)
                p = p->prev_sibling;

            p->prev_sibling = pool->prev_sibling;
        }
    }

    pool_clear(pool);
    free(pool);
}

// Errors

err_t * err_create(DWORD code, const wchar_t *msg)
{
    pool_t *pool = pool_create(NULL);
    err_t *err = pool_calloc(pool, sizeof(*err));

    err->pool = pool;
    err->code = code;
    err->msg = pool_memdup(pool, msg, (wcslen(msg) + 1) * sizeof(wchar_t));

    return err;
}

void err_clear(err_t *err)
{
    pool_destroy(err->pool);
}

const wchar_t * err_str(err_t *err, pool_t *pool)
{
    const wchar_t *sys_msg;
    const wchar_t *str_msg;
    DWORD rc;

    rc = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                       NULL, err->code,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR) &sys_msg, 0, NULL);

    if (rc > 0)
    {
        str_msg = pool_wsprintf(pool, L"%s: %s (error %lu)", err->msg, sys_msg,
                                err->code);
        LocalFree(sys_msg);
    }
    else
    {
        str_msg = pool_wsprintf(pool, L"%s: error %lu", err->msg, err->code);
    }

    return str_msg;
}

// Serial

struct serial {
    const serial_vtable_t *vtable;
    void *priv;
};

err_t * serial_enum(const serial_vtable_t *vtable,
                    serial_enumerate_callback_t callback,
                    void *data,
                    pool_t *pool) {
    return vtable->enumerate(callback, data, pool);
}

err_t * serial_open(serial_t **serial,
                    const serial_vtable_t *vtable,
                    const wchar_t *id,
                    pool_t *pool) {
    *serial = pool_calloc(pool, sizeof(**serial));
    (*serial)->vtable = vtable;
    return vtable->open(&(*serial)->priv, id, pool);
}

err_t * serial_read(serial_t *serial, unsigned char *buffer, size_t *len,
                    pool_t *pool) {
    return serial->vtable->read(serial->priv, buffer, len, pool);
}

err_t * serial_write(serial_t *serial, const unsigned char *data, size_t len,
                     pool_t *pool) {
    return serial->vtable->write(serial->priv, data, len, pool);
}

err_t * serial_read_full(serial_t *serial, unsigned char *buffer, size_t len,
                         int timeout, pool_t *pool) {
    size_t read = 0;
    size_t left = len;
    DWORD start = GetTickCount();

    while (left) {
        size_t chunk = left;
        ERR(serial_read(serial, buffer, &chunk, pool));

        if (chunk) {
            buffer += chunk;
            read += chunk;
            left -= chunk;
            start = GetTickCount();
        } else if (timeout != INFINITE && GetTickCount() - start > timeout) {
            return err_create(ERROR_TIMEOUT,
                              L"Timeout when reading serial device");
        }
    }

    return NULL;
}
