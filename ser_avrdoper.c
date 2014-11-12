/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2003-2004  Theodore A. Roth  <troth@openavr.org>
 * Copyright (C) 2006 Joerg Wunsch <j@uriah.heep.sax.de>
 * Copyright (C) 2006 Christian Starkjohann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id$ */

/*
 * Serial Interface emulation for USB programmer "AVR-Doper" in HID mode.
 */

#include "ac_cfg.h"

#if defined(HAVE_LIBUSB) || defined(WIN32NATIVE)

/* ------------------------------------------------------------------------ */

#define USB_HID_REPORT_TYPE_INPUT   1
#define USB_HID_REPORT_TYPE_OUTPUT  2
#define USB_HID_REPORT_TYPE_FEATURE 3
/* Numeric constants for 'reportType' parameters */

#define USB_ERROR_NONE      0
#define USB_ERROR_ACCESS    1
#define USB_ERROR_NOTFOUND  2
#define USB_ERROR_BUSY      16
#define USB_ERROR_IO        5
/* These are the error codes which can be returned by functions of this
 * module.
 */

typedef void    usbDevice_t;

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

#include <stdio.h>
#include <windows.h>
#include <setupapi.h>

/*
The following is a replacement for hidsdi.h from the Windows DDK. It defines some
of the types and function prototypes of this header for our project. If you
have the Windows DDK version of this file or a version shipped with MinGW, use
that instead.
*/
#ifndef _HIDSDI_H
#define _HIDSDI_H
#include <pshpack4.h>
#include <hidusage.h>
#include <hidpi.h>
typedef struct{
    ULONG   Size;
    USHORT  VendorID;
    USHORT  ProductID;
    USHORT  VersionNumber;
}HIDD_ATTRIBUTES;
void __stdcall      HidD_GetHidGuid(OUT LPGUID hidGuid);
BOOLEAN __stdcall   HidD_GetAttributes(IN HANDLE device, OUT HIDD_ATTRIBUTES *attributes);
BOOLEAN __stdcall   HidD_GetManufacturerString(IN HANDLE device, OUT void *buffer, IN ULONG bufferLen);
BOOLEAN __stdcall   HidD_GetProductString(IN HANDLE device, OUT void *buffer, IN ULONG bufferLen);
BOOLEAN __stdcall   HidD_GetSerialNumberString(IN HANDLE device, OUT void *buffer, IN ULONG bufferLen);
BOOLEAN __stdcall   HidD_GetFeature(IN HANDLE device, OUT void *reportBuffer, IN ULONG bufferLen);
BOOLEAN __stdcall   HidD_SetFeature(IN HANDLE device, IN void *reportBuffer, IN ULONG bufferLen);
BOOLEAN __stdcall   HidD_GetNumInputBuffers(IN HANDLE device, OUT ULONG *numBuffers);
BOOLEAN __stdcall   HidD_SetNumInputBuffers(IN HANDLE device, OUT ULONG numBuffers);
#include <poppack.h>
#endif

#include <hidpi.h>

#ifdef USB_DEBUG
#define DEBUG_PRINT(arg)    printf arg
#else
#define DEBUG_PRINT(arg)
#endif

/* ------------------------------------------------------------------------ */

static void convertUniToAscii(char *buffer)
{
unsigned short  *uni = (void *)buffer;
char            *ascii = buffer;

    while(*uni != 0){
        if(*uni >= 256){
            *ascii++ = '?';
        }else{
            *ascii++ = *uni++;
        }
    }
    *ascii++ = 0;
}

static int usbOpenDevice(usbDevice_t **device, int vendor, char *vendorName, int product, char *productName, int usesReportIDs)
{
GUID                                hidGuid;        /* GUID for HID driver */
HDEVINFO                            deviceInfoList;
SP_DEVICE_INTERFACE_DATA            deviceInfo;
SP_DEVICE_INTERFACE_DETAIL_DATA     *deviceDetails = NULL;
DWORD                               size;
int                                 i, openFlag = 0;  /* may be FILE_FLAG_OVERLAPPED */
int                                 errorCode = USB_ERROR_NOTFOUND;
HANDLE                              handle = INVALID_HANDLE_VALUE;
HIDD_ATTRIBUTES                     deviceAttributes;
				
    HidD_GetHidGuid(&hidGuid);
    deviceInfoList = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    deviceInfo.cbSize = sizeof(deviceInfo);
    for(i=0;;i++){
        if(handle != INVALID_HANDLE_VALUE){
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
        if(!SetupDiEnumDeviceInterfaces(deviceInfoList, 0, &hidGuid, i, &deviceInfo))
            break;  /* no more entries */
        /* first do a dummy call just to determine the actual size required */
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, NULL, 0, &size, NULL);
        if(deviceDetails != NULL)
            free(deviceDetails);
        deviceDetails = malloc(size);
        deviceDetails->cbSize = sizeof(*deviceDetails);
        /* this call is for real: */
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, deviceDetails, size, &size, NULL);
        DEBUG_PRINT(("checking HID path \"%s\"\n", deviceDetails->DevicePath));
        /* attempt opening for R/W -- we don't care about devices which can't be accessed */
        handle = CreateFile(deviceDetails->DevicePath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, openFlag, NULL);
        if(handle == INVALID_HANDLE_VALUE){
            DEBUG_PRINT(("opening failed: %d\n", (int)GetLastError()));
            /* errorCode = USB_ERROR_ACCESS; opening will always fail for mouse -- ignore */
            continue;
        }
        deviceAttributes.Size = sizeof(deviceAttributes);
        HidD_GetAttributes(handle, &deviceAttributes);
        DEBUG_PRINT(("device attributes: vid=%d pid=%d\n", deviceAttributes.VendorID, deviceAttributes.ProductID));
        if(deviceAttributes.VendorID != vendor || deviceAttributes.ProductID != product)
            continue;   /* ignore this device */
        errorCode = USB_ERROR_NOTFOUND;
        if(vendorName != NULL && productName != NULL){
            char    buffer[512];
            if(!HidD_GetManufacturerString(handle, buffer, sizeof(buffer))){
                DEBUG_PRINT(("error obtaining vendor name\n"));
                errorCode = USB_ERROR_IO;
                continue;
            }
            convertUniToAscii(buffer);
            DEBUG_PRINT(("vendorName = \"%s\"\n", buffer));
            if(strcmp(vendorName, buffer) != 0)
                continue;
            if(!HidD_GetProductString(handle, buffer, sizeof(buffer))){
                DEBUG_PRINT(("error obtaining product name\n"));
                errorCode = USB_ERROR_IO;
                continue;
            }
            convertUniToAscii(buffer);
            DEBUG_PRINT(("productName = \"%s\"\n", buffer));
            if(strcmp(productName, buffer) != 0)
                continue;
        }
        break;  /* we have found the device we are looking for! */
    }
    SetupDiDestroyDeviceInfoList(deviceInfoList);
    if(deviceDetails != NULL)
        free(deviceDetails);
    if(handle != INVALID_HANDLE_VALUE){
        *device = (usbDevice_t *)handle;
        errorCode = 0;
    }
    return errorCode;
}

/* ------------------------------------------------------------------------ */

static void    usbCloseDevice(usbDevice_t *device)
{
    CloseHandle((HANDLE)device);
}

/* ------------------------------------------------------------------------ */

static int usbSetReport(usbDevice_t *device, int reportType, char *buffer, int len)
{
HANDLE  handle = (HANDLE)device;
BOOLEAN rval = 0;
DWORD   bytesWritten;

    switch(reportType){
    case USB_HID_REPORT_TYPE_INPUT:
        break;
    case USB_HID_REPORT_TYPE_OUTPUT:
        rval = WriteFile(handle, buffer, len, &bytesWritten, NULL);
        break;
    case USB_HID_REPORT_TYPE_FEATURE:
        rval = HidD_SetFeature(handle, buffer, len);
        break;
    }
    return rval == 0 ? USB_ERROR_IO : 0;
}

/* ------------------------------------------------------------------------ */

static int usbGetReport(usbDevice_t *device, int reportType, int reportNumber, char *buffer, int *len)
{
HANDLE  handle = (HANDLE)device;
BOOLEAN rval = 0;
DWORD   bytesRead;

    switch(reportType){
    case USB_HID_REPORT_TYPE_INPUT:
        buffer[0] = reportNumber;
        rval = ReadFile(handle, buffer, *len, &bytesRead, NULL);
        if(rval)
            *len = bytesRead;
        break;
    case USB_HID_REPORT_TYPE_OUTPUT:
        break;
    case USB_HID_REPORT_TYPE_FEATURE:
        buffer[0] = reportNumber;
        rval = HidD_GetFeature(handle, buffer, *len);
        break;
    }
    return rval == 0 ? USB_ERROR_IO : 0;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */


/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

#include "ser_avrdoper.h"

#define USB_VENDOR_ID   0x16c0
#define USB_PRODUCT_ID  0x05df

extern char *progname;
extern int verbose;

static usbDevice_t  *device = NULL;

/* ------------------------------------------------------------------------- */

static void dumpBlock(char *prefix, unsigned char *buf, int len)
{
int i;

    if(len <= 8){   /* more compact format for short blocks */
        fprintf(stderr, "%s: %d bytes: ", prefix, len);
        for(i = 0; i < len; i++){
            fprintf(stderr, "%02x ", buf[i]);
        }
        fprintf(stderr, " \"");
        for(i = 0; i < len; i++){
            if(buf[i] >= 0x20 && buf[i] < 0x7f){
                fputc(buf[i], stderr);
            }else{
                fputc('.', stderr);
            }
        }
        fprintf(stderr, "\"\n");
    }else{
        fprintf(stderr, "%s: %d bytes:\n", prefix, len);
        while(len > 0){
            for(i = 0; i < 16; i++){
                if(i < len){
                    fprintf(stderr, "%02x ", buf[i]);
                }else{
                    fprintf(stderr, "   ");
                }
                if(i == 7)
                    fputc(' ', stderr);
            }
            fprintf(stderr, "  \"");
            for(i = 0; i < 16; i++){
                if(i < len){
                    if(buf[i] >= 0x20 && buf[i] < 0x7f){
                        fputc(buf[i], stderr);
                    }else{
                        fputc('.', stderr);
                    }
                }
            }
            fprintf(stderr, "\"\n");
            buf += 16;
            len -= 16;
        }
    }
}

static char *usbErrorText(int usbErrno)
{
static char buffer[32];

    switch(usbErrno){
        case USB_ERROR_NONE:    return "Success.";
        case USB_ERROR_ACCESS:  return "Access denied.\nIf running on Linux, check /proc/bus/usb permissions.";
        case USB_ERROR_NOTFOUND:return "Device not found.";
        case USB_ERROR_BUSY:    return "Device is busy.";
        case USB_ERROR_IO:      return "I/O Error.";
        default:
            sprintf(buffer, "Unknown error %d.", usbErrno);
            return buffer;
    }
}

/* ------------------------------------------------------------------------- */

int avrdoper_open(char *port, long baud)
{
int rval;
char *vname = "obdev.at";
char *devname = "AVR-Doper";

    if(device != NULL){ /* we support only one open device */
        fprintf(stderr, "%s: avrdoper_open(): already open, only one open device supported!\n", progname);
        exit(1);
    }
    rval = usbOpenDevice(&device, USB_VENDOR_ID, vname, USB_PRODUCT_ID, devname, 1);
    if(rval != 0){
        fprintf(stderr, "%s: avrdoper_open(): %s\n", progname, usbErrorText(rval));
        exit(1);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */

static int avrdoper_setspeed(int fd, long baud)
{
    /* dummy */
    return 0;
}

/* ------------------------------------------------------------------------- */

void avrdoper_close(int fd)
{
    if(device == NULL){
        fprintf(stderr, "%s: avrdoper_close(): Device never opened (fd=%d)\n", progname, fd);
        exit(1);
    }
    usbCloseDevice(device);
    device = NULL;
}

/* ------------------------------------------------------------------------- */

static int  reportDataSizes[4] = {13, 29, 61, 125};

static int  chooseDataSize(int len)
{
int i;

    for(i = 0; i < sizeof(reportDataSizes)/sizeof(reportDataSizes[0]); i++){
        if(reportDataSizes[i] >= len)
            return i;
    }
    return i - 1;
}

int avrdoper_send(int fd, unsigned char *buf, size_t buflen)
{
    if(verbose > 3)
        dumpBlock("Send", buf, buflen);
    while(buflen > 0){
        unsigned char buffer[256];
        int rval, lenIndex = chooseDataSize(buflen);
        int thisLen = buflen > reportDataSizes[lenIndex] ? reportDataSizes[lenIndex] : buflen;
        buffer[0] = lenIndex + 1;   /* report ID */
        buffer[1] = thisLen;
        memcpy(buffer + 2, buf, thisLen);
        if(verbose > 3)
            fprintf(stderr, "Sending %d bytes data chunk\n", thisLen);
        rval = usbSetReport(device, USB_HID_REPORT_TYPE_FEATURE, (char *)buffer, reportDataSizes[lenIndex] + 2);
        if(rval != 0){
            fprintf(stderr, "%s: avrdoper_send(): %s\n", progname, usbErrorText(rval));
            exit(1);
        }
        buflen -= thisLen;
        buf += thisLen;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */

static unsigned char    avrdoperRxBuffer[280];  /* buffer for receive data */
static int              avrdoperRxLength = 0;   /* amount of valid bytes in rx buffer */
static int              avrdoperRxPosition = 0; /* amount of bytes already consumed in rx buffer */

static void avrdoperFillBuffer(void)
{
int bytesPending = reportDataSizes[1];  /* guess how much data is buffered in device */

    avrdoperRxPosition = avrdoperRxLength = 0;
    while(bytesPending > 0){
        int len, usbErr, lenIndex = chooseDataSize(bytesPending);
        unsigned char buffer[128];
        len = sizeof(avrdoperRxBuffer) - avrdoperRxLength;  /* bytes remaining */
        if(reportDataSizes[lenIndex] + 2 > len) /* requested data would not fit into buffer */
            break;
        len = reportDataSizes[lenIndex] + 2;
        usbErr = usbGetReport(device, USB_HID_REPORT_TYPE_FEATURE, lenIndex + 1, (char *)buffer, &len);
        if(usbErr != 0){
            fprintf(stderr, "%s: avrdoperFillBuffer(): %s\n", progname, usbErrorText(usbErr));
            exit(1);
        }
        if(verbose > 3)
            fprintf(stderr, "Received %d bytes data chunk of total %d\n", len - 2, buffer[1]);
        len -= 2;   /* compensate for report ID and length byte */
        bytesPending = buffer[1] - len; /* amount still buffered */
        if(len > buffer[1])             /* cut away padding */
            len = buffer[1];
        if(avrdoperRxLength + len > sizeof(avrdoperRxBuffer)){
            fprintf(stderr, "%s: avrdoperFillBuffer(): internal error: buffer overflow\n", progname);
            exit(1);
        }
        memcpy(avrdoperRxBuffer + avrdoperRxLength, buffer + 2, len);
        avrdoperRxLength += len;
    }
}

int avrdoper_recv(int fd, unsigned char *buf, size_t buflen)
{
unsigned char   *p = buf;
int             remaining = buflen;
int len = 0;

    if(remaining > 0){
        int available = avrdoperRxLength - avrdoperRxPosition;
        if(available <= 0){ /* buffer is empty */
            avrdoperFillBuffer();
        }
        len = remaining < available ? remaining : available;
        memcpy(p, avrdoperRxBuffer + avrdoperRxPosition, len);
        p += len;
        remaining -= len;
        avrdoperRxPosition += len;

    }
    if(verbose > 3)
        dumpBlock("Receive", buf, len);
    return len;
}

/* ------------------------------------------------------------------------- */

int avrdoper_drain(int fd, int display)
{
    do{
        avrdoperFillBuffer();
    }while(avrdoperRxLength > 0);
    return 0;
}

#endif /* defined(HAVE_LIBUSB) || defined(WIN32NATIVE) */