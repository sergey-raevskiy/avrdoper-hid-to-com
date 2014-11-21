#include <windows.h>
#include <stdio.h>

#include "ser_avrdoper.h"
#include "runtime.h"
#include "stk.h"

char *progname = "avrdoper-hid-to-com";
int verbose = 0;

err_t * run_exchange(const wchar_t *avrdoper_id, pool_t *pool) {
    serial_t *avrdoper;
    serial_t *comport;

    ERR(serial_open(&avrdoper, &avrdoper_hid, avrdoper_id, pool));
    ERR(serial_open(&comport, &com, L"COM3", pool));

    while (1) {
        unsigned char *req, *resp;

        ERR(stk_read_message(&req, comport, INFINITE, pool));
        ERR(stk_write_message(avrdoper, req, pool));

        ERR(stk_read_message(&resp, avrdoper, INFINITE, pool));
        ERR(stk_write_message(comport, resp, pool));
    }
}

int print_dev(void *data, const wchar_t *id, pool_t *pool)
{
    *(err_t **)data = run_exchange(id, pool);
    return 0;
}

static err_t * start(pool_t *pool)
{
    err_t *err = NULL;
    ERR(serial_enum(&avrdoper_hid, print_dev, &err, pool));

    return err;
}

int wmain()
{
    pool_t *pool = pool_create(NULL);
    err_t *err;
    int exit_code;

    err = start(pool);

    if (!err) {
        exit_code = EXIT_SUCCESS;
    } else {
        fwprintf(stderr, L"Error: %s\n", err_str(err, pool));
        err_clear(err);
        exit_code = EXIT_FAILURE;
    }

    pool_destroy(pool);
    return exit_code;
}
