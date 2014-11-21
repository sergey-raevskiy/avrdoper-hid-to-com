#include "runtime.h"

struct serial_private {
    HANDLE port;
};

static err_t * com_open(serial_private_t **dev,
                        const wchar_t *id,
                        pool_t *pool) {
    *dev = pool_calloc(pool, sizeof(**dev));
    (*dev)->port = CreateFile(id,
                             GENERIC_READ | GENERIC_WRITE,
                             0 /* no share */,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             NULL);

    if ((*dev)->port == INVALID_HANDLE_VALUE) {
        return err_create(GetLastError(), L"Can't open COM port");
    }

    return NULL;
}

static err_t * com_read(serial_private_t *dev,
                        unsigned char *buffer,
                        size_t *len,
                        pool_t *pool) {
    DWORD read;
    if (*len > MAXDWORD) {
        *len = MAXDWORD;
    }

    if (ReadFile(dev->port, buffer, *len, &read, NULL)) {
        *len = read;
        return NULL;
    } else {
        return err_create(GetLastError(), L"Can't read COM port");
    }
}

static err_t * com_write(serial_private_t *dev,
                         const unsigned char *data,
                         size_t len,
                         pool_t *pool) {
    DWORD written_dw;
    if (len > MAXDWORD) {
        return err_create(ERROR_INCORRECT_SIZE, L"Length is to big");
    }

    if (WriteFile(dev->port, data, len, &written_dw, NULL)) {
        return NULL;
    } else {
        return err_create(GetLastError(), L"Can't write COM port");
    }
}

const serial_vtable_t com = {
    NULL,
    com_open,
    com_read,
    com_write
};
