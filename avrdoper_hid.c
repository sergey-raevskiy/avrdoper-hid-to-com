#include <windows.h>
#include <hidsdi.h>
#include <SetupAPI.h>

#include "runtime.h"

static void destroy_dev_list(void *data)
{
    SetupDiDestroyDeviceInfoList((HDEVINFO) data);
}

err_t * avrdoper_hid_enum_devices(int (* callback)(const wchar_t *path),
                                  pool_t *pool)
{
    GUID hid_guid;
    HDEVINFO dev_list;
    DWORD  member;

    HidD_GetHidGuid(&hid_guid);

    dev_list = SetupDiGetClassDevs(&hid_guid, NULL, NULL,
                                   DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

    if (dev_list == INVALID_HANDLE_VALUE) {
        return err_create(GetLastError(), L"Can't enumerate HID devices");
    }

    pool_add_cleanup(pool, destroy_dev_list, dev_list);

    for (member = 0; ; member++) {
        SP_DEVICE_INTERFACE_DATA info;
        SP_DEVICE_INTERFACE_DETAIL_DATA *details;
        HANDLE handle;
        DWORD size;
        BOOL rc;

        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);

        rc = SetupDiEnumDeviceInterfaces(dev_list, NULL, &hid_guid, member,
                                         &info);

        if (!rc && GetLastError() == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (!rc) {
            return err_create(GetLastError(), L"Can't enumerate HID devices");
        }

        rc = SetupDiGetDeviceInterfaceDetail(dev_list, &info, NULL, 0, &size,
                                             NULL);

        if (!rc && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return err_create(GetLastError(), L"Can't query interface details");
        }

        details = pool_calloc(pool, size);
        ZeroMemory(details, sizeof(*details));
        details->cbSize = sizeof(*details);

        rc = SetupDiGetDeviceInterfaceDetail(dev_list, &info, details, size,
                                             &size, NULL);

        if (!rc) {
            return err_create(GetLastError(), L"Can't query interface details");
        }

        callback(details->DevicePath);
    }

    return NULL;
}
