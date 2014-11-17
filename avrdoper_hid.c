#include <windows.h>
#include <hidsdi.h>
#include <SetupAPI.h>

#include "avrdoper_hid.h"

#define AVRDOPER_VID 0x16c0
#define AVRDOPER_PID 0x05df

static err_t * get_interface_details(SP_DEVICE_INTERFACE_DETAIL_DATA **details,
                                     HDEVINFO devs,
                                     SP_DEVICE_INTERFACE_DATA *info,
                                     pool_t *pool)
{
    BOOL rc;
    DWORD size;

    rc = SetupDiGetDeviceInterfaceDetail(devs, info, NULL, 0, &size, NULL);

    if (!rc && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return err_create(GetLastError(), L"Can't query interface details");
    }

    (*details) = pool_calloc(pool, size);
    (*details)->cbSize = sizeof(**details);

    rc = SetupDiGetDeviceInterfaceDetail(devs, info, *details, size, &size,
                                         NULL);

    if (!rc) {
        return err_create(GetLastError(), L"Can't query interface details");
    }

    return NULL;
}

static void destroy_dev_list(void *data)
{
    SetupDiDestroyDeviceInfoList((HDEVINFO) data);
}

static void close_handle(void *data)
{
    CloseHandle((HANDLE) data);
}

static DWORD open_device(HANDLE *handle, const wchar_t *path, pool_t *pool)
{
    *handle = CreateFile(path,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL /*lpSecurityAttributes*/,
                         OPEN_EXISTING,
                         0 /*dwFlagsAndAttributes*/,
                         NULL /*hTemplateFile*/);

    if (*handle != INVALID_HANDLE_VALUE) {
        pool_add_cleanup(pool, close_handle, *handle);
        return ERROR_SUCCESS;
    } else {
        return GetLastError();
    }
}

err_t * avrdoper_hid_enum_devices(avrdoper_enum_callback_t callback,
                                  void *data,
                                  pool_t *pool)
{
    GUID hid_guid;
    HDEVINFO dev_list;
    DWORD  member;
    pool_t *iterpool;

    HidD_GetHidGuid(&hid_guid);

    dev_list = SetupDiGetClassDevs(&hid_guid, NULL, NULL,
                                   DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

    if (dev_list == INVALID_HANDLE_VALUE) {
        return err_create(GetLastError(), L"Can't enumerate HID devices");
    }

    pool_add_cleanup(pool, destroy_dev_list, dev_list);

    iterpool = pool_create(pool);
    for (member = 0; ; member++) {
        SP_DEVICE_INTERFACE_DATA info;
        SP_DEVICE_INTERFACE_DETAIL_DATA *details;
        HIDD_ATTRIBUTES attributes;
        HANDLE handle;
        DWORD size;
        BOOL rc;

        pool_clear(iterpool);

        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);

        rc = SetupDiEnumDeviceInterfaces(dev_list, NULL, &hid_guid, member,
                                         &info);

        if (!rc && GetLastError() == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (!rc) {
            return err_create(GetLastError(), L"Can't enumerate HID devices");
        }

        ERR(get_interface_details(&details, dev_list, &info, pool));

        if (open_device(&handle, details->DevicePath, iterpool) 
            != ERROR_SUCCESS) {
            continue;;
        }

        if (!HidD_GetAttributes(handle, &attributes)) {
            return err_create(GetLastError(), L"Can't query device attributes");
        }

        if (attributes.VendorID != AVRDOPER_VID
            || attributes.ProductID != AVRDOPER_PID) {
                continue;
        }

        if (!callback(data, details->DevicePath, iterpool))
            break;
    }
    pool_destroy(iterpool);

    return NULL;
}

struct avrdoper_dev {
    HANDLE handle;
    char rxbuf[256];
    int rxpos;
    int rxavail;
};

static const size_t report_sizes[] = { 13, 29, 61, 125 };

static const int choose_data_size(size_t size)
{
    int i = 0;
    while (size > report_sizes[i] && i < _countof(report_sizes))
        i++;

    return i;
}

err_t * avrdoper_hid_open(avrdoper_dev_t **dev,
                          const wchar_t *id,
                          pool_t *pool)
{
    DWORD err;

    (*dev) = pool_calloc(pool, sizeof(**dev));
    if (err = open_device(&(*dev)->handle, id, pool)) {
        return err_create(err, L"Can't open device");
    }

    (*dev)->rxavail = 0;
    (*dev)->rxpos = 0;

    return NULL;
}

err_t * avrdoper_hid_write(avrdoper_dev_t *dev,
                           const char *data,
                           size_t len,
                           pool_t *pool)
{
    while (len) {
        int id = choose_data_size(len);
        int chunk_len = len > report_sizes[id] ? report_sizes[id] : len;
        char buf[256];

        buf[0] = (char) (id + 1);
        buf[1] = (char) chunk_len;
        memcpy(buf + 2, data, chunk_len);

        if (!HidD_SetFeature(dev->handle, buf, report_sizes[id] + 2)) {
            return err_create(GetLastError(), L"Can't write to device");
        }

        len -= chunk_len;
        data += chunk_len;
    }

    return NULL;
}

err_t * avrdoper_hid_read(avrdoper_dev_t *dev,
                          char *buffer,
                          size_t *len,
                          pool_t *pool)
{
    size_t left = *len;
    size_t read = 0;

    while (left) {
        size_t recv_sz = dev->rxavail > left ? left : dev->rxavail;
        int id;

        memcpy(buffer, dev->rxbuf + dev->rxpos, recv_sz);
        buffer += recv_sz;
        read += recv_sz;
        left -= recv_sz;
        dev->rxpos += recv_sz;
        dev->rxavail -= recv_sz;

        id = choose_data_size(left);
        dev->rxbuf[0] = (char) (id + 1);

        if (!dev->rxavail && left) {
            if (!HidD_GetFeature(dev->handle, dev->rxbuf,
                                 report_sizes[id] + 2)) {
                return err_create(GetLastError(), L"Can't write to device");
            }

            dev->rxavail = dev->rxbuf[1];
            dev->rxpos = 2;

            if (!dev->rxavail) {
                break;
            }
        }
    }

    *len = read;
    return NULL;
}
