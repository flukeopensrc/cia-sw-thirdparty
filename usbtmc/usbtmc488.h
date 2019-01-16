/*
 * Copyright (C) 2010 Tektronix, Inc.
 * 
 * Author: Mike Brant <michael.s.brant@tektronix.com>
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#ifndef USBTMC488_H
#define USBTMC488_H


#ifdef __cplusplus
extern "C" {
#endif

#undef __STRICT_ANSI__
#include <asm/byteorder.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>

#ifndef BOOL
#define BOOL        unsigned char
#endif

#ifndef TRUE 
#define TRUE    1
#endif

#ifndef FALSE 
#define FALSE   0
#endif

#define USBTMC488_DEVINFO_STRING_BUFSIZE   128
#define USBTMC_STRING_DESCR_MAX_LENGTH     126

/* Information required for a USBTMC-USB488 device interface
*
* NOTE: For all string parameters, the following characters are not
*       allowed: double quote ("), asterisk (*), forward slash (/),
*       backslash (\), colon (:), and question mark (?). Also, no leading
*       or trailing spaces are allowed. All characters must be printable
*       ASCII characters (in the range of 0x20 through 0x7e).
* NOTE: Only English language (language ID 0x0409) is supported.
*
*   vendorId
*     Specifies the vendor identification of the device. This
*     value is assigned by the USB-IF (USB Implementers Forum).
*   productId
*     Specifies the product identification of the device. This
*     value is assigned by the device manufacturer.
*   productVersion
*     Binary-coded decimal that indicates the version of the
*     product. For example, 0x0100 indicates revision 1.00
*   manufacturerDescr
*     String describing the manufacturer of the device. Must be <= 63 chars.
*   productDescr
*     String describing the device (e.g. model name). Must be <= 63 chars.
*   productSerialNumberDescr
*     String describing the serial number of the device. Must be <= 63 chars.
*   configurationDescr
*     String describing the current configuration of the device. Must be
*     <= 63 chars. Usually "Standard".
*   interfaceDescr
*     String describing the current interface of the device.
*     For devices using this gadget driver, the string should be
*     "USBTMC-USB488 Interface". Must be <= 63 chars.
*   maxPower
*     Specifies the maximum power consumption of the USB device
*     from the bus in this specific configuration when the device
*     is fully operational. Expressed in 2 mA units (i.e., 50 =
*     100 mA).
*   selfPowered
*     Indicates whether this USB configuration is self powered
*     or bus powered. Setting this value false indicates that
*     this is a bus-powered device.
*/
typedef struct
{
    __u16  vendorId;
    __u16  productId;
    __u16  productVersion;
    char   manufacturerDescr[USBTMC488_DEVINFO_STRING_BUFSIZE];
    char   productDescr[USBTMC488_DEVINFO_STRING_BUFSIZE];
    char   productSerialNumberDescr[USBTMC488_DEVINFO_STRING_BUFSIZE];
    char   configurationDescr[USBTMC488_DEVINFO_STRING_BUFSIZE];
    char   interfaceDescr[USBTMC488_DEVINFO_STRING_BUFSIZE];
    __u8   maxPower;
    int    selfPowered;
} USBTMC488_DEVICE_INFO;

typedef enum
{
    USBTMC488_SUCCESS                   = 0,
    USBTMC488_FAILED                    = 1,
    USBTMC488_INTERFACE_ALREADY_ENABLED = 2,
    USBTMC488_INTERFACE_NOT_ENABLED     = 3,
    USBTMC488_NO_DATA_REQUESTED         = 4,
    USBTMC488_NUM_STATUSES              = 5
} USBTMC488_STATUS;

typedef enum
{
    USBTMC488_MSG_DEVICE_CLEAR              = 0,
    USBTMC488_MSG_DATA_WITHOUT_EOM          = 1,
    USBTMC488_MSG_DATA_WITH_EOM             = 2,
    USBTMC488_MSG_DATA_REQUESTED            = 3,
    USBTMC488_MSG_DEVICE_TRIGGER            = 4,
    USBTMC488_MSG_UPDATE_STATUS_BYTE        = 5,
    USBTMC488_MSG_REQUEST_SERVICE           = 6,
    USBTMC488_MSG_REN_ENABLE                = 7,
    USBTMC488_MSG_REN_DISABLE               = 8,    
    USBTMC488_MSG_GOTO_LOCAL                = 9,    
    USBTMC488_MSG_CANCEL_IO                 = 10,
    USBTMC488_MSG_DEVICE_CLEAR_DONE         = 11,
    USBTMC488_MSG_FREE_REQUEST_MSG          = 12,
    USBTMC488_MSG_GOTO_LOCAL_LOCKOUT        = 13,
    USBTMC488_MSG_UPDATE_LOCAL_STATUS_BYTE  = 14,    
    USBTMC488_NUM_MSG_TYPES                 = 15
} USBTMC488_MSG_TYPE;

typedef struct
{
    __u32   length;
    __u8   *buffer;
} USBTMC488_MSG_BUFFER;

typedef struct
{
    USBTMC488_MSG_TYPE    type;
        __u8                  value;
    USBTMC488_MSG_BUFFER *msg_buffer;
} USBTMC488_MESSAGE;

#define USBTMC_MAX_THREADS 3
#define USBTMC_MAX_THREADNAME_LENGTH 64
typedef struct
{
    unsigned int  pid;
    char     name[USBTMC_MAX_THREADNAME_LENGTH+1];
} USBTMC488_THREAD_INFO;

extern void usbtmc488_tsprintf( const char *pFmt, ... );

extern USBTMC488_STATUS usbtmc488_get_thread_info(unsigned int *returnedNumThreads, const USBTMC488_THREAD_INFO **pThreadArray);
extern USBTMC488_STATUS usbtmc488_enable_interface( const  USBTMC488_DEVICE_INFO *device_info, void   (*event_handler)(const USBTMC488_MESSAGE *msg, void* pData), void* pData);
extern USBTMC488_STATUS usbtmc488_disable_interface( void );
extern BOOL usbtmc488_interface_enabled( void );

/* Common function for:
 *     1. Sending data messages with or without EOM. The USBTMC488_MESSAGE
 *        "type" field is expected to be one of:
 *            USBTMC488_MSG_DATA_WITHOUT_EOM
 *            USBTMC488_MSG_DATA_WITH_EOM
 *        and the "value" field is ignored. The "msg_buffer" field is expected
 *        to point to a valid USBTMC488_MSG_BUFFER structure.
 *     2. Updating the status byte. The USBTMC488_MESSAGE "type" field is set to
 *        USBTMC488_MSG_UPDATE_STATUS_BYTE. The status byte is expected to be
 *        provided in the "value" field of the USBTMC488_MESSAGE and the
 *        "msg_buffer" field should be NULL.
 *     3. Requesting service. The USBTMC488_MESSAGE "type" field is set to
 *        USBTMC488_MSG_REQUEST_SERVICE. The status byte is expected to be
 *        provided in the "value" field of the USBTMC488_MESSAGE and the
 *        "msg_buffer" field should be NULL.
 *     4. Cancel I/O. Any pending output is canceled, internal driver queues
 *        are flushed. The USBTMC488_MESSAGE "type" field is set to 
 *        USBTMC488_MSG_CANCEL_IO, the "length" and "value" fields are expected
 *        to be 0 and the "msg_buffer" field should be NULL.
 *     5. Informing driver that device clear processing is complete. The
 *        USBTMC488_MESSAGE "type" field is set to
 *        USBTMC488_MSG_DEVICE_CLEAR_DONE, the "length" and "value" fields are
 *        expected to be 0 and the "msg_buffer" field should be NULL.
 *     6. Informing the driver that the message sent to the application
 *        (from a message with type USBTMC488_MSG_DATA_WITHOUT_EOM,
 *        USBTMC488_MSG_DATA_WITH_EOM or USBTMC488_DATA_REQUESTED) can be freed.
 *        USBTMC488_MESSAGE "type" field is set to USBTMC488_MSG_FREE_REQUEST_MSG,
 *        the "length" and "value" fields are expected to be 0 and the "msg_buffer"
 *        field should be NULL. Until this message has been received, no further
 *        messages from the host can be read.
 *
 * Following are the valid USBTMC488_MSG_TYPE message types for calls to this
 * function. Any other type results in a USBTMC488_FAILED return value.
 *     USBTMC488_MSG_DATA_WITHOUT_EOM
 *     USBTMC488_MSG_DATA_WITH_EOM
 *     USBTMC488_MSG_UPDATE_STATUS_BYTE
 *     USBTMC488_MSG_REQUEST_SERVICE
 *     USBTMC488_MSG_CANCEL_IO
 *     USBTMC488_MSG_DEVICE_CLEAR_DONE
 *     USBTMC488_MSG_FREE_REQUEST_MSG
*/
extern USBTMC488_STATUS usbtmc488_message( const USBTMC488_MESSAGE *msg );
extern const char* usbtmc488_status_to_string(USBTMC488_STATUS status);
extern const char* usbtmc488_msg_type_to_string(USBTMC488_MSG_TYPE type);

#define USBTMC488_API          (1 << 0)
#define USBTMC488_EP_BULKIN    (1 << 1)
#define USBTMC488_EP_BULKOUT   (1 << 2)
#define USBTMC488_EP_CONTROL   (1 << 3)
#define USBTMC488_EP_INTRPT    (1 << 4)
#define USBTMC488_EVENTS       (1 << 5)
#define USBTMC488_SYNCH        (1 << 6)
#define USBTMC488_SYSTEM       (1 << 7)
#define USBTMC488_THREADS      (1 << 8)

extern void usbtmc488_set_verbosity(unsigned int verbosity);
extern unsigned int usbtmc488_get_verbosity( void );
extern const char * usbtmc488_show_verbosity_bits( void );

#ifdef __cplusplus
}
#endif

#endif /* USBTMC488_H */
