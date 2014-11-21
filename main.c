#include <windows.h>
#include <stdio.h>

#include "ser_avrdoper.h"
#include "runtime.h"
#include "stk.h"

char *progname = "avrdoper-hid-to-com";
int verbose = 0;

int print_dev(void *data, const wchar_t *id, pool_t *pool)
{
    err_t *err;
    serial_t *dev;
    int i;
    unsigned char msg[] = { 
        0x1b,       // start
        0x01,       // seq
        0x00, 0x01, // size
        0x0e,       // token
        0x01,       // command
        0x00
    };
    unsigned char *response;

    for (i = 0; i < _countof(msg) - 1; i++) {
        msg[_countof(msg) - 1] ^= msg[i];
    }

    err = serial_open(&dev, &avrdoper_hid, id, pool);
    if (err) {
        *(err_t **)data = err;
        return 0;
    }

    err = stk_write_message(dev, msg, pool);
    if (err) {
        *(err_t **)data = err;
        return 0;
    }

    err = stk_read_message(&response, dev, INFINITE, pool);
    if (err) {
        *(err_t **)data = err;
        return 0;
    }

    response[16] = 0;
    printf("%s\n", response + 7);

    return 1;
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

    //hPort = CreateFileA("COM3", FILE_GENERIC_READ | FILE_GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    //if (hPort == INVALID_HANDLE_VALUE)
    //    die_win32();

    //fd = avrdoper_open(NULL, 0);
    //read_completed = CreateEventA(NULL, FALSE, TRUE, NULL);
    //if (!read_completed)
    //    die_win32();

    //write_completed = CreateEventA(NULL, FALSE, TRUE, NULL);
    //if (!write_completed)
    //    die_win32();

    //while (1) {
    //    HANDLE handles[] = { read_completed, write_completed };
    //    DWORD wait = WaitForMultipleObjectsEx(_countof(handles), handles, FALSE, INFINITE, TRUE);

    //    if (wait == WAIT_OBJECT_0 + 0) {
    //        // read
    //        ZeroMemory(&read_io.ov, sizeof(read_io.ov));
    //        read_io.ov.hEvent = read_completed;

    //        if (!ReadFileEx(hPort, read_io.buf, sizeof(read_io.buf), &read_io.ov, read_completion))
    //            die_win32();
    //    } else if (wait == WAIT_OBJECT_0 + 1) {
    //        // write
    //        int len;
    //        ZeroMemory(&write_io.ov, sizeof(write_io.ov));
    //        write_io.ov.hEvent = write_completed;

    //        len = avrdoper_recv(0, (unsigned char *)write_io.buf, sizeof(write_io.buf));
    //       if (len ) {
    //           printf("prog to comp: ");
    //           hexdump(write_io.buf, len);
    //           printf("\n");

    //           if (!WriteFileEx(hPort, write_io.buf, len, &write_io.ov, write_completion))
    //               die_win32();
    //       } else {
    //           SetEvent(write_completed);
    //       }

    //    } else if (wait == WAIT_FAILED) {
    //        die_win32();
    //    } else if (wait == WAIT_IO_COMPLETION) {
    //        continue;
    //    }
    //}
}
