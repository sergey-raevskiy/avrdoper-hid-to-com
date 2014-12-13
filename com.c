#include "runtime.h"

struct serial_private {
    HANDLE port;
};

static err_t * com_open(serial_private_t **dev,
                        const wchar_t *id,
                        pool_t *pool) {
    DCB dcb;

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

    // NOTE: This might be not the best place for doing this.
    memset(&dcb, 0, sizeof(dcb));
    if (!BuildCommDCB(L"9600,N,8,1", &dcb)) {
        return err_create(GetLastError(), L"Can't set COM port state");
    }

    if (!SetCommState((*dev)->port, &dcb)) {
        return err_create(GetLastError(), L"Can't set COM port state");
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
