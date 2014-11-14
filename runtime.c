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

void pool_clear(pool_t *pool)
{
    while (pool->child)
    {
        pool_t *child = pool->child;
        pool->child = pool->child->prev_sibling;

        pool_destroy(child);
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
