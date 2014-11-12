#include <windows.h>
#include <stdio.h>

#include "ser_avrdoper.h"

char *progname = "avrdoper-hid-to-com";
int verbose = 0;

static void die_win32()
{
    char *msg;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                   NULL, GetLastError(), LANG_USER_DEFAULT, (LPSTR) &msg, 0, NULL);
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

typedef struct {
    OVERLAPPED ov;
    char buf[1024];
} io_t;

static void hexdump(const char  *b, int len) {
    while (len--) {
        printf("%02x ", (unsigned int)*b++);
    }
}

VOID CALLBACK read_completion(
    _In_     DWORD dwErrorCode,
    _In_     DWORD dwNumberOfBytesTransfered,
    _Inout_  LPOVERLAPPED lpOverlapped
    )
{
    io_t *io = CONTAINING_RECORD(lpOverlapped, io_t, ov);
    printf("comp to prog: ");
    hexdump(io->buf, dwNumberOfBytesTransfered);
    printf("\n");

    avrdoper_send(0, (unsigned char *)io->buf, dwNumberOfBytesTransfered);
    SetEvent(io->ov.hEvent);
}

VOID CALLBACK write_completion(
    _In_     DWORD dwErrorCode,
    _In_     DWORD dwNumberOfBytesTransfered,
    _Inout_  LPOVERLAPPED lpOverlapped
    )
{
    io_t *io = CONTAINING_RECORD(lpOverlapped, io_t, ov);
    SetEvent(io->ov.hEvent);
}


int main()
{
    HANDLE hPort, read_completed, write_completed;
    io_t read_io, write_io;
    int fd;
   
    hPort = CreateFileA("COM3", FILE_GENERIC_READ | FILE_GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hPort == INVALID_HANDLE_VALUE)
        die_win32();

    fd = avrdoper_open(NULL, 0);
    read_completed = CreateEventA(NULL, FALSE, TRUE, NULL);
    if (!read_completed)
        die_win32();

    write_completed = CreateEventA(NULL, FALSE, TRUE, NULL);
    if (!write_completed)
        die_win32();

    while (1) {
        HANDLE handles[] = { read_completed, write_completed };
        DWORD wait = WaitForMultipleObjectsEx(_countof(handles), handles, FALSE, INFINITE, TRUE);

        if (wait == WAIT_OBJECT_0 + 0) {
            // read
            ZeroMemory(&read_io.ov, sizeof(read_io.ov));
            read_io.ov.hEvent = read_completed;

            if (!ReadFileEx(hPort, read_io.buf, sizeof(read_io.buf), &read_io.ov, read_completion))
                die_win32();
        } else if (wait == WAIT_OBJECT_0 + 1) {
            // write
            int len;
            ZeroMemory(&write_io.ov, sizeof(write_io.ov));
            write_io.ov.hEvent = write_completed;

            len = avrdoper_recv(0, (unsigned char *)write_io.buf, sizeof(write_io.buf));
           if (len ) {
               printf("prog to comp: ");
               hexdump(write_io.buf, len);
               printf("\n");

               if (!WriteFileEx(hPort, write_io.buf, len, &write_io.ov, write_completion))
                   die_win32();
           } else {
               SetEvent(write_completed);
           }

        } else if (wait == WAIT_FAILED) {
            die_win32();
        } else if (wait == WAIT_IO_COMPLETION) {
            continue;
        }
    }

    system("pause");

    return 0;
}
