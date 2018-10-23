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

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/version.h>
#include <linux/usb/functionfs.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>

#include "usbtmc488.h"

#define ELEMENTAL_SUPERSPEED

#define USB_SUBCLASS_TMC        3
#define USBTMC_USB488           1


#define INTERRUPT_IN_MAXPACKET  2
/* bInterval is expressed in microframes. The microframe rates are:
 *     125 microseconds for HS
 *       1 millsecond   for FS
 *
 * The actual polling interval is computed as 2**(bInterval-1)*(microframe rate).
 *
 */
#define INTERRUPT_IN_INTERVAL_FS   2
#define INTERRUPT_IN_INTERVAL_HS   2
#define INTERRUPT_IN_INTERVAL_SS   2

/* For bulk endpoint reads and writes, loop forever when read/write
 * returns EINTR and retry up to 10 times when read/write returns
 * EAGAIN (resource temporarily busy).
 */
#define BULK_FAILURE_RETRY(expression)                                    \
    (__extension__                                                        \
    ({ long int __result, __retryCount = 10;                              \
       do                                                                 \
       {                                                                  \
           errno = 0;                                                     \
/*sjz           printf("Calling expression...\n");     */                        \
           __result = (long int) (expression);                            \
/*sjz            printf("result = %d, errno = %d\n", (int)__result, errno);    */ \
           if ( __result == -1L && errno == EAGAIN )                      \
           {                                                              \
           __retryCount--;                                                \
           }                                                              \
       }                                                                  \
       while ((__result == -1L && errno == EINTR) ||                      \
          (__result == -1L && errno == EAGAIN && __retryCount));          \
    __result; }))

/* To enable use of a debugger, the following macro is used to expose
 * the otherwise static functions and data.
 *   #define PRIVATE
 *
 * For production use, this should be defined as:
 *   #define PRIVATE static
 */
#undef  PRIVATE
#define PRIVATE static
#if 0
#define PRIVATE
#endif

/* The kernel layer of the gadget driver for the dwc_otg device handles
 * reset of data toggle for USB_REQ_CLEAR_FEATURE USB_ENDPOINT_HALT.
 * For devices that don't do this, change the #undef below to enable
 * handle_control() to make the appropriate ioctl() calls.
 */
#undef NOKERNEL_CLEAR_HALT

#if PTHREAD_STACK_MIN < (1024 * 16)
    #define USBTMC488_STACK_SIZE (1024 * 16)
#else /* PTHREAD_STACK_MIN */
    #define USBTMC488_STACK_SIZE PTHREAD_STACK_MIN
#endif /* PTHREAD_STACK_MIN */

/* Current gadgetfs implementations for the processors we support do not
 * support ioctl() calls for GADGETFS_FIFO_STATUS or GADGETFS_FIFO_FLUSH.
 * The places where such calls are made are conditioned on the following
 * #defines.
 */
#undef FUNCTIONFS_FIFO_STATUS_IMPLEMENTED
#undef FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED

/* Initialize semaphores once. They remain initialized for the life of
 * the process. This is called once in usbtmc488_enable_interface()
 * and should never be called again.
 */
PRIVATE int initialize_driver( void );

/* This is set in initialize_driver() and should never be set again.
 */
PRIVATE BOOL driver_initialized = FALSE;

PRIVATE BOOL device_connected = FALSE;

PRIVATE USBTMC488_DEVICE_INFO current_device_info;

/* The maximum endpoint transfer size depends on the underlying
 * device implementation. The implementation below is specific
 * to the dwc_otg PCD implementation provided originally by
 * Synopsys and revised with bug fixes by DENX. We (Tektronix)
 * fixed the default maximum transfer sizes based on the size
 * of the 'endpoint transfer size' register, which is 10 bits:
 *
 * HS = ((2**10)-1) * 512) = 1023 * 512 = 523776
 * FS = ((2**10)-1) * 64)  = 1023 *  64 =  65472
 */
/*
Could you try changing your MAX_TRANSFER_SIZE_<HS|FS> to 511*512
and 511*64 (instead of 1023*512 and 1023*64)? I think that the
maximum packet count should be 1023 but the code seems to have
the module parameter set to max at 511 for some reason. I'm
wondering if there is a good reason for that.
 */

#if __arm__
    #define MAX_TRANSFER_SIZE_HS (16*1024)
    #define MAX_TRANSFER_SIZE_FS (16*1024)
#elif __powerpc__
    #define MAX_TRANSFER_SIZE_HS (511*512)
    #define MAX_TRANSFER_SIZE_FS (511*64)
#else
    #define MAX_TRANSFER_SIZE_HS (32*1024)
    #define MAX_TRANSFER_SIZE_FS (32*1024)
#endif

#define ALIGNMENT  4

#define MAX_PACKET_SIZE_SS	1024
#define MAX_PACKET_SIZE_HS	512
#define MAX_PACKET_SIZE_FS	64

/* Upon connection, we malloc the buffers used for writing bulk-in messages
 * based on the connection speed (HS or not HS). We set MaxTransferSize to
 * MAX_TRANSFER_SIZE_HS or MAX_TRANSFER_SIZE_FS accordingly.
 *
 * Upon disconnection, the buffers are freed and MaxTransferSize is set to 0.
 */
PRIVATE __u32    MaxTransferSize	= 0;
PRIVATE __u32    MaxPacketSize		= 0;

/* Binary coded decimal field indicating the USB
 * specification level used in the design of this device. As
 * specified in USB 2.0 specification, section 9.6.1.
 */

#if defined(ELEMENTAL_SUPERSPEED)

#define DEVICE_DESCR_USBSPEC_LEVEL		0x0300
#define DEVICE_DESCR_MAX_PACKET_SIZE	9

#endif /* ELEMENTAL_SUPERSPEED */


/* NOTE:  these IDs don't imply endpoint numbering; host side drivers
 * should use endpoint descriptors, or perhaps bcdDevice, to configure
 * such things.  Other product IDs could have different policies.
 */

/*-------------------------------------------------------------------------*/
/* Bulk-In Message IDs */

#define USBTMC_REQ_INITIATE_ABORT_BULK_OUT        1
#define USBTMC_REQ_CHECK_ABORT_BULK_OUT_STATUS    2
#define USBTMC_REQ_INITIATE_ABORT_BULK_IN         3
#define USBTMC_REQ_CHECK_ABORT_BULK_IN_STATUS     4
#define USBTMC_REQ_INITIATE_CLEAR                 5
#define USBTMC_REQ_CHECK_CLEAR_STATUS             6
#define USBTMC_REQ_GET_CAPABILITIES               7
#define USBTMC_REQ_INDICATOR_PULSE               64
#define USB488_REQ_READ_STATUS_BYTE             128
#define USB488_REQ_REN_CONTROL                  160
#define USB488_REQ_GO_TO_LOCAL                  161
#define USB488_REQ_LOCAL_LOCKOUT                162

/*-------------------------------------------------------------------------*/
/* Bulk-Out Message IDs */

#define DEV_DEP_MSG_OUT                           1
#define REQUEST_DEV_DEP_MSG_IN                    2
#define DEV_DEP_MSG_IN                            2
#define VENDOR_SPECIFIC_OUT                     126
#define REQUEST_VENDOR_SPECIFIC_IN              127
#define VENDOR_SPECIFIC_IN                      127
#define USB488_TRIGGER                          128

/*-------------------------------------------------------------------------*/

#define STATUS_SUCCESS                          0x01
#define STATUS_PENDING                          0x02
#define STATUS_INTERRUPT_IN_BUSY                0x20
#define STATUS_FAILED                           0x80
#define STATUS_TRANSFER_NOT_IN_PROGRESS         0x81
#define STATUS_SPLIT_NOT_IN_PROGRESS            0x82
#define STATUS_SPLIT_IN_PROGRESS                0x83

/*-------------------------------------------------------------------------*/

#define ENDPOINT_HALT          -2
#define ENDPOINT_ERROR         -1
#define ENDPOINT_IDLE           0
#define ENDPOINT_INPROGRESS     1

/*-------------------------------------------------------------------------*/

#define BULKIN_ENDPOINT_ADDRESS    1
#define BULKOUT_ENDPOINT_ADDRESS   2
#define INTRPTIN_ENDPOINT_ADDRESS  3

/*-------------------------------------------------------------------------*/

#define LANGUAGE_ID_ENGLISH_US  0x0409

#define CONFIG_SELF_POWERED     0x40

/* these descriptors are modified based on what controller we find */

#define START_STRINGID          1
#define STRINGID_MFGR           1
#define STRINGID_PRODUCT        2
#define STRINGID_SERIAL         3
#define STRINGID_CONFIG         4
#define STRINGID_INTERFACE      5
#define NUM_STRINGIDS           5

PRIVATE unsigned int verbosity                            = 0x0;

static const char *verbosity_table =
    "API        0x00000001\n"
    "BULKIN EP  0x00000002\n"
    "BULKOUT EP 0x00000004\n"
    "CONTROL EP 0x00000008\n"
    "INTRPT EP  0x00000010\n"
    "EVENTS     0x00000020\n"
    "SYNCH      0x00000040\n"
    "SYSTEM     0x00000080\n"
    "THREADS    0x00000100\n";

PRIVATE BOOL usbtmc_interface_enabled                     = FALSE;
PRIVATE void (*usbtmc_event_handler)(const USBTMC488_MESSAGE *msg, void* pData);
PRIVATE __u8  usbtmc_status_byte                          = 0;

void* pData;        // if the call back function handler needs something...pass it here.

/* Timeout, in msec, used for writes to the interrupt-in endpoint.
 * A timeout is needed for cases where the host is not sending
 * IN tokens on this endpoint.
 */
#define INTRPT_TIMEOUT          50

/* Timeout, in msec, used when waiting for the application to send the
 * USBTMC488_MSG_DEVICE_CLEAR_DONE event.
 */
#define DEVICE_CLEAR_DONE_TIMEOUT 1000

/* These semaphores are initialized as part of driver initialization
 * done in initialize_driver().
 */
PRIVATE sem_t bulkinLock;
PRIVATE sem_t bulkoutLock;
PRIVATE sem_t debugLock;
PRIVATE sem_t deviceClearLock;
PRIVATE sem_t intrptDoneEvent;
PRIVATE sem_t intrptEvent;
PRIVATE sem_t intrptLock;
PRIVATE sem_t requestLock;
PRIVATE sem_t responseLock;
PRIVATE sem_t responseReady;
PRIVATE sem_t statusByteLock;
PRIVATE sem_t stringtabLock;
PRIVATE sem_t tsPrintLock;
PRIVATE sem_t userServiceLock;

typedef enum
{
    BULKIN_LOCK               = 0,
    BULKOUT_LOCK              = 1,
    DEVICE_CLEAR_LOCK         = 2,
    INTRPT_DONE_EVENT         = 3,
    INTRPT_EVENT              = 4,
    INTRPT_LOCK               = 5,
    REQUEST_LOCK              = 6,
    RESPONSE_LOCK             = 7,
    STATUS_BYTE_LOCK          = 8,
    STRINGTAB_LOCK            = 9,
    TALKADDR_OR_TERMINATE     = 10,
    USER_SERVICE_LOCK         = 11,
    NUM_LOCKS                 = 12
} USBTMC488_LOCK_TYPE;

PRIVATE const char *lockTypeStrings[NUM_LOCKS] =
{
    "BULKIN_LOCK",
    "BULKOUT_LOCK",
    "DEVICE_CLEAR_LOCK",
    "INTRPT_DONE_EVENT",
    "INTRPT_EVENT",
    "INTRPT_LOCK",
    "REQUEST_LOCK",
    "RESPONSE_LOCK",
    "STATUS_BYTE_LOCK",
    "STRINGTAB_LOCK",
    "TALKADDR_OR_TERMINATE",
    "USER_SERVICE_LOCK"
};

PRIVATE sem_t *pLockArray[NUM_LOCKS] =
{
    &bulkinLock,
    &bulkoutLock,
    &deviceClearLock,
    &intrptDoneEvent,
    &intrptEvent,
    &intrptLock,
    &requestLock,
    &responseLock,
    &responseReady,
    &statusByteLock,
    &stringtabLock,
    &userServiceLock
};

PRIVATE struct usb_device_descriptor device_desc =
{
    .bLength =              sizeof(device_desc),
    .bDescriptorType =      USB_DT_DEVICE,

    .bcdUSB =               __constant_cpu_to_le16(DEVICE_DESCR_USBSPEC_LEVEL),
    .bDeviceClass =         USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass =      0,
    .bDeviceProtocol =      0,
    /* .idVendor =       ... set by usbtmc488_enable_interface() */
    /* .idProduct =      ... set by usbtmc488_enable_interface() */
    /* .bcdDevice =      ... set by usbtmc488_enable_interface() */
    .bMaxPacketSize0 =      DEVICE_DESCR_MAX_PACKET_SIZE,
    .idVendor =             0,
    .idProduct =            0,
    .bcdDevice =            0,
    .iManufacturer =        STRINGID_MFGR,
    .iProduct =             STRINGID_PRODUCT,
    .iSerialNumber =        STRINGID_SERIAL,
    .bNumConfigurations =   1
};

/* The value used for the bConfigurationValue field of the configuration
 * descriptor. The USBTMC spec requires that this be 1.
 */
#define CONFIG_VALUE		1

#if defined(ELEMENTAL_SUPERSPEED)

PRIVATE struct usb_descriptors_s {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
    struct usb_functionfs_descs_head_v2 header;
    __le32 fs_count;
    __le32 hs_count;
    __le32 ss_count;
#else /* LINUX_VERSION_CODE */
    __le32 magic;
    __le32 length;
    __le32 flags;
    __le32 fs_count;
    __le32 hs_count;
    __le32 ss_count;
#endif /* LINUX_VERSION_CODE */
    struct {
        struct usb_interface_descriptor intf;
        struct usb_endpoint_descriptor_no_audio bulkin;
        struct usb_endpoint_descriptor_no_audio bulkout;
        struct usb_endpoint_descriptor_no_audio intr;
    } __attribute__((packed)) fs_descs, hs_descs;
    struct {
        struct usb_interface_descriptor intf;
        struct usb_endpoint_descriptor_no_audio bulkin;
        struct usb_ss_ep_comp_descriptor bulkin_comp;
        struct usb_endpoint_descriptor_no_audio bulkout;
        struct usb_ss_ep_comp_descriptor bulkout_comp;
        struct usb_endpoint_descriptor_no_audio intr;
        struct usb_ss_ep_comp_descriptor intr_comp;
    } __attribute__((packed)) ss_descs;
} __attribute__((packed)) descriptors = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
    .header = {
        .magic = __cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
        .length = __cpu_to_le32(sizeof descriptors),
        .flags = __cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC | FUNCTIONFS_HAS_SS_DESC),
    },
    .fs_count = __cpu_to_le32(4),
    .hs_count = __cpu_to_le32(4),
    .ss_count = __cpu_to_le32(7),
#else /* LINUX_VERSION_CODE */
    .magic = __cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
    .length = __cpu_to_le32(sizeof descriptors),
    .flags = __cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC | FUNCTIONFS_HAS_SS_DESC),
    .fs_count = __cpu_to_le32(4),
    .hs_count = __cpu_to_le32(4),
    .ss_count = __cpu_to_le32(7),
#endif /* LINUX_VERSION_CODE */
    .fs_descs = {
        .intf = {
            .bLength = sizeof descriptors.fs_descs.intf,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_APP_SPEC,
            .bInterfaceSubClass = USB_SUBCLASS_TMC,
            .bInterfaceProtocol = USBTMC_USB488,
            .iInterface = STRINGID_INTERFACE,
        },
        .bulkin = {
            .bLength = sizeof descriptors.fs_descs.bulkin,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | BULKIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_FS),
            .bInterval = 0,
        },
        .bulkout = {
            .bLength = sizeof descriptors.fs_descs.bulkout,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_OUT | BULKOUT_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_FS),
            .bInterval = 0,
        },
        .intr = {
            .bLength = sizeof descriptors.fs_descs.intr,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | INTRPTIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = __cpu_to_le16(INTERRUPT_IN_MAXPACKET),
            .bInterval = INTERRUPT_IN_INTERVAL_FS,
        },
    },
    .hs_descs = {
        .intf = {
            .bLength = sizeof descriptors.hs_descs.intf,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_APP_SPEC,
            .bInterfaceSubClass = USB_SUBCLASS_TMC,
            .bInterfaceProtocol = USBTMC_USB488,
            .iInterface = STRINGID_INTERFACE,
        },
        .bulkin = {
            .bLength = sizeof descriptors.hs_descs.bulkin,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | BULKIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_HS),
            .bInterval = 0,
        },
        .bulkout = {
            .bLength = sizeof descriptors.hs_descs.bulkout,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_OUT | BULKOUT_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_HS),
            .bInterval = 0,
        },
        .intr = {
            .bLength = sizeof descriptors.hs_descs.intr,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | INTRPTIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = __cpu_to_le16(INTERRUPT_IN_MAXPACKET),
            .bInterval = INTERRUPT_IN_INTERVAL_HS,
        },
    },
    .ss_descs = {
        .intf = {
            .bLength = sizeof descriptors.ss_descs.intf,
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_APP_SPEC,
            .bInterfaceSubClass = USB_SUBCLASS_TMC,
            .bInterfaceProtocol = USBTMC_USB488,
            .iInterface = STRINGID_INTERFACE,
        },
        .bulkin = {
            .bLength = sizeof descriptors.ss_descs.bulkin,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | BULKIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_SS),
            .bInterval = 0,
        },
        .bulkin_comp = {
            .bLength = sizeof descriptors.ss_descs.bulkin_comp,
            .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
            .bMaxBurst = 0,
            .bmAttributes = 0,
            .wBytesPerInterval = 0,
        },
        .bulkout = {
            .bLength = sizeof descriptors.ss_descs.bulkout,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_OUT | BULKOUT_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = __cpu_to_le16(MAX_PACKET_SIZE_SS),
            .bInterval = 0,
        },
        .bulkout_comp = {
            .bLength = sizeof descriptors.ss_descs.bulkout_comp,
            .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
            .bMaxBurst = 0,
            .bmAttributes = 0,
            .wBytesPerInterval = 0,
        },
        .intr = {
            .bLength = sizeof descriptors.ss_descs.intr,
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = USB_DIR_IN | INTRPTIN_ENDPOINT_ADDRESS,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = __cpu_to_le16(INTERRUPT_IN_MAXPACKET),
            .bInterval = INTERRUPT_IN_INTERVAL_SS,
        },
        .intr_comp = {
            .bLength = sizeof descriptors.ss_descs.intr_comp,
            .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
            .bMaxBurst = 0,
            .bmAttributes = 0,
            .wBytesPerInterval = __cpu_to_le16(INTERRUPT_IN_MAXPACKET),
        },
    },
};

#endif /* ELEMENTAL_SUPERSPEED */

/*-------------------------------------------------------------------------*/
/* USBTMC Bulk message headers */
struct usbtmc_bulk_out_hdr
{
    __u8    bMsgID;
    __u8    bTag;
    __u8    bTagInv;
    __u8    bReserve;
};

struct usbtmc_bulk_out_msg
{
    struct usbtmc_bulk_out_hdr      sHdr;
    __u32                           lTransferSize;
    __u8                            bmTransfer;
    __u8                            bRes[3];
};

struct usbtmc_bulk_in_hdr
{
    __u8    bMsgID;
    __u8    bTag;
    __u8    bTagInv;
    __u8    bRes;
};

struct usbtmc_bulk_in_msg
{
    struct usbtmc_bulk_in_hdr       sHdr;
    __u32                           lTransferSize;
    __u8                            bmTransAttribute;
    __u8                            bRes[3];
};

struct usbtmc488_req_dev_dep_msg
{
    struct usbtmc_bulk_in_hdr       sHdr;
    __u32                           lTransferSize;
    __u8                            bmTransAttribute;
    __u8                            termChar;
    __u8                            bRes[2];
};

/*-------------------------------------------------------------------------*/
/* USBTMC control response
*/
PRIVATE struct usbtmc_get_cap_resp
{
    __u8    bStatus;
    __u8    bRes1;
    __u8    bUSBTMCVersion[2];
    __u8    bUSBTMCIfaceCap;
    __u8    bUSBTMCDevCap;
    __u8    bRes2[6];
    __u8    bUSB488Version[2];
    __u8    bUSB488IfaceCap;
    __u8    bUSB488DevCap;
    __u8    bRes3[8];
} usbtmc_get_cap_resp =
{
    .bStatus =          STATUS_SUCCESS,
    .bRes1 =            0,
    .bUSBTMCVersion =   { 0x00, 0x01}, /* 1.0 */
    .bUSBTMCIfaceCap =  0, /* Does not accept indicator pulse; not talk-only;
                  * not listen-only */
    .bUSBTMCDevCap =    0, /* Bulk-in end not supported by termchar match */
    .bRes2 =            { 0, 0, 0, 0, 0, 0 },
    .bUSB488Version =   { 0x00, 0x01}, /* 1.0 */
    .bUSB488IfaceCap =  0x07, /* USB488 +
                             * REN_CONTROL/GO_TO_LOCAL/LOCAL_LOCKOUT +
                 * TRIGGER
                 */
    .bUSB488DevCap =    0x05, /* SR1 + DT1 */
    .bRes3 =            { 0, 0, 0, 0, 0, 0, 0, 0 },
};

PRIVATE struct usbtmc_short_resp
{
    __u8    bStatus;
    __u8    bData;
} usbtmc_short_resp =
{
    .bStatus =    0,
    .bData =      0
};

PRIVATE struct usbtmc_check_abort_resp
{
    __u8    bStatus;
    __u8	bmAbortBulkIn;
    __u8    bRes[2];
    __u32   lNbytesXD;
} usbtmc_check_abort_resp;

PRIVATE struct usbtmc_read_sb_resp
{
    __u8    bStatus;
    __u8    bTag;
    __u8    bStatusByte;
} usbtmc_read_sb_resp;

PRIVATE struct usbtmc_interrupt_in_resp
{
    __u8    bNotify1;
    __u8    bNotify2;
} usbtmc_interrupt_in_resp;

/* For writing response data and handling bulkin requests. These
 * data items are protected by the RESPONSE_LOCK semaphore.
 */
PRIVATE struct usbtmc_bulk_in_msg  last_req_dev_dep_msgin_msg;

typedef enum
{
    IDLE           = 0,
    DATA_REQUESTED = 1,
    IN_PROGRESS    = 2,
    TERMINATE      = 3
} RESPONSE_STATES;

PRIVATE RESPONSE_STATES responseState = IDLE;

typedef struct
{
    struct usbtmc_bulk_in_msg  header;
    __u8                       data[];
} ResponseMsgType;

/* malloc'd in usbtmc488_enable_interface()
 * and freed in usbtmc488_disable_interface().
 */
PRIVATE ResponseMsgType *pResponseMsg = NULL;


/* For handling bulkout requests */
PRIVATE struct usbtmc_bulk_out_hdr  last_bulkout_hdr;

/* malloc'd in usbtmc488_enable_interface()
 * and freed in usbtmc488_disable_interface().
 */
PRIVATE __u8            *pRequestBuffer = NULL;

/*-------------------------------------------------------------------------*/

struct usb_functionfs_stringtab {
    struct usb_functionfs_strings_head header;
    __u16 language;
    char str[];
} __attribute__((packed));

struct usb_functionfs_stringtab *strings;

/*-------------------------------------------------------------------------*/

PRIVATE int      HIGHSPEED_CAPABLE;
PRIVATE char	*EP0_NAME      = NULL,
                *EP_IN_NAME   = NULL,
                *EP_OUT_NAME  = NULL,
                *EP_INTR_NAME = NULL;

PRIVATE enum usb_device_speed    current_speed;

PRIVATE int      full_speed_packet_size, high_speed_packet_size;

PRIVATE inline unsigned int min(unsigned int a, unsigned int b)
{
    return(a < b) ? a : b;
}

PRIVATE inline unsigned int max(unsigned int a, unsigned int b)
{
    return(a > b) ? a : b;
}

/*----------------- LOCAL FUNCTION PROTOTYPES----------------*/
PRIVATE void usbtmcDbgPrint(int bit, const char *fmt, ...);
PRIVATE int autoconfig();
PRIVATE void close_fd(void *fd_ptr);
PRIVATE int ep_config(char *name, const char *callingFunc);
PRIVATE void process_dev_dep_msg_in(struct usbtmc_bulk_in_msg *msg);
PRIVATE USBTMC488_STATUS queue_response_msg( const USBTMC488_MESSAGE *usbtmc488_msg );
PRIVATE void usbtmc_intrpt_thread_cleanup(void*);
PRIVATE void usbtmc_request_thread_cleanup(void*);
PRIVATE void * usbtmc_request_thread(void *param);
PRIVATE void * usbtmc_intrpt_thread(void *param);
PRIVATE int start_io();
PRIVATE void stop_io();
PRIVATE int init_device(void);
PRIVATE int handle_tmc_control(int fd, struct usb_ctrlrequest *setup);
PRIVATE void handle_control(int fd, struct usb_ctrlrequest *setup);
PRIVATE void * ep0_thread(void *param);
PRIVATE void usbtmc488_sleep( unsigned int numMilliseconds );
PRIVATE int timed_wait( USBTMC488_LOCK_TYPE lockType, int msecs );

typedef enum
{
    USBTMC488_EP0_THREAD       = 0,
    USBTMC488_BULKOUT_THREAD   = 1,
    USBTMC488_INTRPT_THREAD    = 2,
    USBTMC488_NUM_THREADS      = 3
} ThreadArrayEntry;

PRIVATE USBTMC488_THREAD_INFO usbtmcThreadArray[USBTMC488_NUM_THREADS];

#define WAITFOR(type)                                                     \
    sem_status = 0;                                                   \
    usbtmcDbgPrint(USBTMC488_EVENTS, "%s: WAITFOR: %s: ENTRY\n",      \
        __FUNCTION__, lockTypeStrings[type]);                     \
                                                                          \
    if ( 0 != (sem_status = sem_wait(pLockArray[type])) )             \
    {                                                                 \
        sem_errno = errno;                                        \
        usbtmc488_tsprintf(                                       \
            "usbtmc488: %s: error locking %s semaphore\n"     \
            "  error: %d (%s)\n",                             \
            __FUNCTION__,                                     \
            lockTypeStrings[type],                            \
            sem_status,                                       \
            strerror(sem_errno));                             \
    }                                                                 \
                                                                          \
    usbtmcDbgPrint(USBTMC488_EVENTS, "%s: WAITFOR: %s: EXIT\n",       \
        __FUNCTION__, lockTypeStrings[type]);

#define UNLOCK(type)                                                      \
    sem_status = 0;                                                   \
    usbtmcDbgPrint(USBTMC488_SYNCH, "%s: UNLOCK: %s: ENTRY\n",        \
        __FUNCTION__, lockTypeStrings[type]);                     \
                                                                          \
    sem_status = sem_trywait(pLockArray[type]);                       \
    if ( (0 != sem_status) && (EAGAIN != errno) )                     \
    {                                                                 \
        sem_errno = errno;                                        \
        usbtmc488_tsprintf(                                       \
            "usbtmc488: %s: sem_trywait error for"            \
            "  %s semaphore\n"                                \
            "  error: %d (%s)\n",                             \
            __FUNCTION__,                                     \
            lockTypeStrings[type],                            \
            sem_status,                                       \
            strerror(sem_errno));                             \
    }                                                                 \
    if ( 0 != (sem_status = sem_post(pLockArray[type])) )             \
    {                                                                 \
        sem_errno = errno;                                        \
        usbtmc488_tsprintf(                                       \
            "usbtmc488: %s: error unlocking %s semaphore\n"   \
            "  error: %d (%s)\n",                             \
            __FUNCTION__,                                     \
            lockTypeStrings[type],                            \
            sem_status,                                       \
            strerror(sem_errno));                             \
    }                                                                 \
                                                                          \
    usbtmcDbgPrint(USBTMC488_SYNCH, "%s: UNLOCK: %s: EXIT\n",         \
        __FUNCTION__, lockTypeStrings[type]);

#define SIGNAL_EVENT	UNLOCK

#define UNSIGNAL_EVENT(type)                                                \
    sem_status = 0;                                                     \
    usbtmcDbgPrint(USBTMC488_EVENTS, "%s: UNSIGNAL_EVENT: %s: ENTRY\n", \
        __FUNCTION__, lockTypeStrings[type]);                       \
                                                                            \
    sem_status = sem_trywait(pLockArray[type]);                         \
    if ( (0 != sem_status) && (EAGAIN != errno) )                       \
    {                                                                   \
        sem_errno = errno;                                          \
        usbtmc488_tsprintf(                                         \
            "usbtmc488: %s: error unsignaling %s\n"             \
            "  error: %d (%s)   \n",                            \
            __FUNCTION__,                                       \
            lockTypeStrings[type],                              \
            sem_status,                                         \
            strerror(sem_errno));                               \
    }                                                                   \
                                                                            \
    usbtmcDbgPrint(USBTMC488_EVENTS, "%s: UNSIGNAL_EVENT: %s: EXIT\n",  \
        __FUNCTION__, lockTypeStrings[type]);

PRIVATE void
usbtmcDbgPrint(int bit, const char *fmt, ...)
{
    int sem_status = 0, sem_errno = 0;
    if ( !(verbosity & bit) )
    {
        return;
    }
    if ( TRUE == driver_initialized )
    {
        if ( 0 != (sem_status = sem_wait(&debugLock)) )
        {
            sem_errno = errno;
            usbtmc488_tsprintf(
                "usbtmc488: %s: error locking debugLock semaphore\n"
                "  error: %d (%s)\n",
                __FUNCTION__,
                sem_status,
                strerror(sem_errno));
        }
    }
    va_list ap;

    if ( verbosity & bit )
    {
        usbtmc488_tsprintf("usbtmc488: ");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        fflush(stdout);
    }
    if ( TRUE == driver_initialized )
    {
        if ( 0 != (sem_status = sem_post(&debugLock)) )
        {
            sem_errno = errno;
            usbtmc488_tsprintf(
                "usbtmc488: %s: error unlocking debugLock semaphore\n"
                "  error: %d (%s)\n",
                __FUNCTION__,
                sem_status,
                strerror(sem_errno));
        }
    }
}

PRIVATE int
autoconfig()
{
    // CJS - How to tell we're using net2280?
    usbtmcDbgPrint(USBTMC488_SYSTEM, "%s: net2280\n", __FUNCTION__);
    HIGHSPEED_CAPABLE = 1;

    EP0_NAME = (char *) "/dev/gadget/ep0";
    EP_IN_NAME = (char *) "/dev/gadget/ep1";
    EP_OUT_NAME = (char *) "/dev/gadget/ep2";
    EP_INTR_NAME = (char *) "/dev/gadget/ep3";
    MaxTransferSize = MAX_TRANSFER_SIZE_HS;
    MaxPacketSize = MAX_PACKET_SIZE_HS;
    full_speed_packet_size = __le16_to_cpu(MAX_PACKET_SIZE_FS);
    high_speed_packet_size = __le16_to_cpu(MAX_PACKET_SIZE_HS);

    return(0);
}

/*-------------------------------------------------------------------------*/

typedef enum
{
    BULKIN_IDLE       = 0,
    BULKIN_INPROGRESS = 1,
    BULKIN_HALT       = 2
} BULKIN_STATUS;

PRIVATE BULKIN_STATUS bulkin_inprogress = BULKIN_IDLE;

typedef enum
{
    BULK_EP_ERROR,
    BULK_EP_IDLE,
    BULK_EP_INPROGRESS,
    BULK_EP_HALT
} BULK_EP_STATE;

PRIVATE BULK_EP_STATE	bulkin_state      = BULK_EP_ERROR;
PRIVATE BULK_EP_STATE	bulkout_state     = BULK_EP_ERROR;

PRIVATE pthread_t		ep0_tid         = (pthread_t)-1;
PRIVATE pthread_t		bulkout_tid     = (pthread_t)-1;
PRIVATE pthread_t		intr_tid        = (pthread_t)-1;

PRIVATE int				ep0_fd          = -ENXIO;
PRIVATE int				bulkin_fd       = -ENXIO;
PRIVATE int				bulkout_fd      = -ENXIO;
PRIVATE int				intr_fd         = -ENXIO;

PRIVATE void
close_fd(void *fd_ptr)
{
#if (FUNCTIONFS_FIFO_STATUS_IMPLEMENTED && FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED)
    int     status;
#endif /* FUNCTIONFS_FIFO_STATUS_IMPLEMENTED && FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED */
    int     fd;

    fd = *(int *)fd_ptr;
    *(int *)fd_ptr = -EINVAL;

    if ( -EINVAL == fd )
    {
        usbtmc488_tsprintf("%s: invalid file descriptor\n", __FUNCTION__);
        return;
    }

#if (FUNCTIONFS_FIFO_STATUS_IMPLEMENTED && FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED)
    /* test the FIFO ioctls (non-ep0 code paths) */
    if ( pthread_self() != ep0_tid )
    {
        status = ioctl(fd, FUNCTIONFS_FIFO_STATUS);
        if ( status < 0 )
        {
            /* ENODEV reported after disconnect */
            if ( errno != ENODEV && errno != EOPNOTSUPP  && errno != ESHUTDOWN )
            {
                usbtmc488_tsprintf("%s: get fifo status error for %d: %d (%s)\n",
                    __FUNCTION__, fd, errno, strerror(errno));
            }
        }
        else
        {
            usbtmc488_tsprintf(
                "usbtmc488: %s: fd %d, unclaimed = %d\n",
                __FUNCTION__, fd, status);
            if ( status )
            {
                status = ioctl(fd, FUNCTIONFS_FIFO_FLUSH);
                if ( status < 0 )
                {
                    usbtmc488_tsprintf("%s: fifo flush error for %d: %d (%s)\n",
                        __FUNCTION__, fd, errno, strerror(errno));
                }
            }
        }
    }
#endif /* FUNCTIONFS_FIFO_STATUS_IMPLEMENTED && FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED */

    if ( close(fd) < 0 )
    {
        usbtmc488_tsprintf("%s: error closing %d: errno=%d (%s)\n",
            __FUNCTION__, fd, errno, strerror(errno));
    }
}


/* you should be able to open and configure endpoints
 * whether or not the host is connected
 */
PRIVATE int
ep_config( char *name, const char *callingFunc )
{
    int             fd, status;

    /* open and initialize with endpoint descriptor(s) */
    fd = open(name, O_RDWR);
    if ( fd < 0 )
    {
        status = -errno;
        usbtmc488_tsprintf("usbtmc488: %s: %s: open %s error %d (%s)\n",
            __FUNCTION__, callingFunc, name, errno, strerror(errno));
        return(status);
    }

    return(fd);
}

#define bulkin_open(name) \
    ep_config(name, __FUNCTION__)
#define bulkout_open(name) \
    ep_config(name, __FUNCTION__)
#define intr_open(name) \
    ep_config(name, __FUNCTION__)

PRIVATE void
process_dev_dep_msg_in(struct usbtmc_bulk_in_msg *msg)
{
    __u32                transferSize = __le32_to_cpu(msg->lTransferSize);
    int                  sem_status   = 0,
                         sem_errno    = 0;
    USBTMC488_MSG_BUFFER eventMsgBuffer;
    USBTMC488_MESSAGE    eventMsg;

    __u8    *data = (__u8 *)(msg+1);

    usbtmcDbgPrint(	USBTMC488_EP_BULKIN,
            "%s:\n"
            "    Header->bMsgID     = %d\n"
            "    Header->bTag       = 0x%x\n"
            "    Header->bTagInv    = 0x%x\n"
            "    lTransferSize      = %d\n"
            "    bmTransAttribute   = %d\n",
            __FUNCTION__,
            msg->sHdr.bMsgID,
            msg->sHdr.bTag,
            msg->sHdr.bTagInv,
            transferSize,
            msg->bmTransAttribute);

    WAITFOR(RESPONSE_LOCK);
    if ( REQUEST_DEV_DEP_MSG_IN == msg->sHdr.bMsgID )
    {
        eventMsg.type   = USBTMC488_MSG_DATA_REQUESTED;
        /* Copy the request header for use in the response message */
        memcpy(	&last_req_dev_dep_msgin_msg,
            msg,
            sizeof(last_req_dev_dep_msgin_msg));
        responseState = DATA_REQUESTED;
        SIGNAL_EVENT(TALKADDR_OR_TERMINATE);
        data = NULL;
    }
    else
    {
        responseState = IDLE;
        if ( msg->bmTransAttribute & 0x01 )
        {
            eventMsg.type = USBTMC488_MSG_DATA_WITH_EOM;
        }
        else
        {
            eventMsg.type = USBTMC488_MSG_DATA_WITHOUT_EOM;
        }
    }
    UNLOCK(RESPONSE_LOCK);

    eventMsg.value = 0;
    eventMsgBuffer.length = transferSize;
    eventMsgBuffer.buffer = data;
    eventMsg.msg_buffer   = &eventMsgBuffer;

    usbtmc_event_handler(&eventMsg, pData);

    return;
}

/* queue_response_msg:
 *
 * This function is executed via the application layer in its context when
 * response data is available for output. The application calls the USBTMC
 * gadget API function usbtmc488_message() with a message type of either
 * USBTMC488_MSG_DATA_WITHOUT_EOM or USBTMC488_MSG_DATA_WITH_EOM, resulting
 * in this function being called.
 *
 * No data can be output until the host requests it via the REQUEST_DEV_DEP_MSG_IN
 * message on the bulk-out endpoint. When such a message is received, the
 * TALKADDR_OR_TERMINATE event is signalled and the global variable responseState
 * is set to DATA_REQUESTED. It's also possible that any pending or in-progress
 * bulk-out transfer is to be terminated. We must handle these conditions as
 * well.
 *
 * The terminology "TALKADDR" stands for "talk-addressed" and is from the IEEE488.2
 * protocol terminology that means that the host has sent a "byte request" message
 * to this device's "talk-address". In our USBTMC context, it means that the host
 * has sent us a REQUEST_DEV_DEP_MSG_IN message.
 *
 * Following are the causal conditions and the values of responseState when the
 * TALKADDR_OR_TERMINATE event is signalled:
 *
 *    Condition                            responseState
 *    -----------------------------------  --------------------------------
 *    REQUEST_DEV_DEP_MSG_IN message       DATA_REQUESTED
 *    received on bulk-out EP.
 *
 *    USBTMC_REQ_INITIATE_ABORT_BULK_IN    TERMINATE
 *    message received on control EP.
 *
 *    USBTMC_REQ_INITIATE_CLEAR message    TERMINATE
 *    received on control EP.
 *
 *    USBTMC488_MSG_CANCEL_IO message      TERMINATE
 *    received from application via API
 *    call to usbtmc488_message().
 *
 * The buffer containing the data for output is provided by the application
 * and may be any size. The maximum amount of data that we can output in
 * a given transfer is specified by the host in the REQUEST_DEV_DEP_MSG_IN message.
 * This message is kept in the thread-safe global struct last_req_dev_dep_msgin_msg.
 * The local variable request_size represents this maximum value. So, it's
 * possible that the application may have more data to output than the host is
 * able to consume in a single transfer. In such cases, we need to send the
 * amount of data the host requested and then wait for the host to request more
 * data (when it sends a REQUEST_DEV_DEP_MSG_IN message) or to request that we abort
 * the transfer. This level of the protocol is handled by the outer while loop in
 * this function ( while ( !bDone ) ). The completion of a transfer is indicated
 * to the USB host by the USBTMC message header bmTransAttribute bit: 1 = EOM
 * (end of message).
 *
 * This function can be called with only one of the following message types:
 *     USBTMC488_MSG_DATA_WITHOUT_EOM
 *         This message type indicates that the buffer of this message is a
 *         portion of the complete response and implies that more response
 *         data will be subsequently queued.
 *     USBTMC488_MSG_DATA_WITH_EOM
 *         This message type indicates that the buffer of this message completes
 *         the response.
 *
 * We must copy the data provided by the application for output because:
 *     1. Every transfer must have a message header prepended to the data.
 *     2. All buffers must begin on a 4-byte boundary and be a multiple of 4 bytes
 *        in size. Therefore, alignment bytes may be needed.
 *
 * The USBTMC gadget uses an allocated structure for this purpose. This structure
 * has a standard 12-byte message header member and the data buffer member. The size
 * of the data buffer member is based on the underlying hardware, as determined
 * by the definitions of MAX_TRANSFER_SIZE_HS and MAX_TRANSFER_SIZE_FS, and includes
 * 4 additional bytes for alignment. The maximum number of bytes that can be stored
 * in the data buffer member for transfer is MaxTransferSize. The global pointer
 * to this structure is pResponseMsg.
 *
 * So, a transfer may be broken into several transactions with smaller data payloads
 * that can fit in the allocated response buffer (pResponseMsg->data). The amount of
 * data sent with these transactions is computed in the local variable payloadSize.
 * This level of the transfer is handled by the inner while loop in this function
 * (  while ( transferSize > 0 ) ).
 *
 * We also must conform to the short packet protocol requirement which says that if
 * the last data payload in a transfer is a full packet (e.g. 512 bytes for HS or
 * 64 bytes for FS), then we must send a short packet afterwards. A short packet is
 * a standard usbtmc_bulk_in_msg that is all zeroes.
 *
 * Scenarios:
 *     nQ => number of data bytes to be queued (usbtmc488_msg->msg_buffer->length)
 *     nP => maximum number of data bytes per payload
 *           (MaxTransferSize-sizeof(struct usbtmc_bulk_in_msg))
 *     nR => number of bytes requested from host (request_size)
 *
 *     Scenario 1: nQ <= nP
 *                    <= nR
 *     Scenario 2: nQ <= nP
 *                    >  nR
 *     Scenario 3: nQ >  nP
 *                    <= nR
 *     Scenario 4: nQ >  nP
 *                    >  nR
 */
PRIVATE USBTMC488_STATUS
queue_response_msg( const USBTMC488_MESSAGE *usbtmc488_msg )
{
    int                        sem_status        = 0,
                               sem_errno         = 0;
        int                        localErrno        = 0;
    WAITFOR(RESPONSE_LOCK);

    int                        status            = 0;
    __u32                      request_size      = 0;
    __u32                      bytes_remaining   = usbtmc488_msg->msg_buffer->length;
    __u8                      *pInputBuffer      = usbtmc488_msg->msg_buffer->buffer;
    __u32                      transferSize      = 0;
    __u32                      payloadSize       = 0;
    __u32                      bytesToWrite      = 0;
    __u8                      *pWriteBuffer;
    struct usbtmc_bulk_in_msg  short_packet;
    __u32                      max_packet_size   =
            current_speed == USB_SPEED_HIGH ?
                high_speed_packet_size : full_speed_packet_size;
    BOOL                       bShortPacket      = FALSE;
    BOOL                       bDone             = FALSE;
    size_t                     padBytes          = 0;
    USBTMC488_STATUS retStatus                   = USBTMC488_SUCCESS;

    if ( 0 == MaxTransferSize )
    {
        retStatus = USBTMC488_INTERFACE_NOT_ENABLED;
        bDone = TRUE;
    }

    UNLOCK(RESPONSE_LOCK);
    while ( !bDone )
    {
        WAITFOR(TALKADDR_OR_TERMINATE);
        WAITFOR(RESPONSE_LOCK);
        if ( NULL == pResponseMsg )
        {
            usbtmc488_tsprintf("%s: disconnected\n", __FUNCTION__);
            bDone = TRUE;
            continue;
        }
        if ( (DATA_REQUESTED != responseState) &&
             (IN_PROGRESS != responseState)
           )
        {
            responseState = TERMINATE;
            retStatus = USBTMC488_NO_DATA_REQUESTED;
            usbtmcDbgPrint(USBTMC488_EP_BULKIN,
                "%s: device is not talk addressed\n", __FUNCTION__);
            UNLOCK(BULKIN_LOCK);
            bDone = TRUE;
            continue;
        }
        else
        {
            responseState = IN_PROGRESS;
            bulkin_state = BULK_EP_INPROGRESS;
            bulkin_inprogress = BULKIN_INPROGRESS;
        }

        request_size = __le32_to_cpu(last_req_dev_dep_msgin_msg.lTransferSize);
        memcpy( &(pResponseMsg->header),
            &last_req_dev_dep_msgin_msg,
            sizeof(pResponseMsg->header) );

        transferSize = min(request_size, bytes_remaining);
        payloadSize = min(transferSize, MaxTransferSize-sizeof(struct usbtmc_bulk_in_msg));
        bytesToWrite = payloadSize + sizeof(struct usbtmc_bulk_in_msg);
        pResponseMsg->header.lTransferSize = __cpu_to_le32(transferSize);
        pResponseMsg->header.bRes[0] =
            pResponseMsg->header.bRes[1] =
            pResponseMsg->header.bRes[2] = 0;
        if ( (transferSize == bytes_remaining) &&
             (usbtmc488_msg->type == USBTMC488_MSG_DATA_WITH_EOM) )
        {
            pResponseMsg->header.bmTransAttribute = 0x01;
        }
        else
        {
            pResponseMsg->header.bmTransAttribute = 0x00;
        }

        /* Write the message header and data */
        WAITFOR(BULKIN_LOCK);
        pWriteBuffer = (__u8 *)pResponseMsg;

        while ( transferSize > 0 )
        {
            memcpy(pResponseMsg->data, pInputBuffer, (size_t) payloadSize);
            pInputBuffer += payloadSize;

            padBytes  = (ALIGNMENT - ((payloadSize)&(ALIGNMENT-1)));
            if ( padBytes  == ALIGNMENT )
            {
                padBytes  = 0;
            }
            if ( 0 != padBytes )
            {
                memset(pResponseMsg->data+payloadSize, 0, padBytes);
            }

            if ( (bytesToWrite == max_packet_size) ||
                 ((bytesToWrite > max_packet_size) && (bytesToWrite % max_packet_size) == 0) )
            {
                bShortPacket = TRUE;
            }
            else
            {
                bShortPacket = FALSE;
            }

            status = BULK_FAILURE_RETRY(
                 write(bulkin_fd,
                       pWriteBuffer,
                       bytesToWrite)
                 );
            fsync(bulkin_fd);

            if ( status != (int)bytesToWrite )
            {
                localErrno = errno;
                usbtmc488_tsprintf("%s: write error: wrote %d of %d bytes\n"
                           "  localErrno=%d (%s)\n",
                    __FUNCTION__,
                    status,
                    bytesToWrite,
                    localErrno,
                    strerror(localErrno));
                bulkin_inprogress = BULKIN_IDLE;
                bulkin_state = BULK_EP_HALT;
                UNLOCK(BULKIN_LOCK);
                responseState = IDLE;
                UNLOCK(RESPONSE_LOCK);
                return(USBTMC488_FAILED);
            }
            transferSize      -= payloadSize;
            bytes_remaining   -= payloadSize;
            pWriteBuffer       = pResponseMsg->data;
            bytesToWrite       = payloadSize = min(transferSize, MaxTransferSize);
        }

        if ( 0 == bytes_remaining )
        {
            UNLOCK(BULKIN_LOCK);
            bDone = TRUE;
        }

        if ( bShortPacket )
        {
            usbtmcDbgPrint(USBTMC488_EP_BULKIN,
                "%s: Sending short packet\n", __FUNCTION__);
            /* Terminate the transfer with a short packet */
            memset((__u8 *)&short_packet, 0, sizeof(short_packet));
            short_packet.sHdr.bMsgID = DEV_DEP_MSG_IN;
            status = BULK_FAILURE_RETRY(write(bulkin_fd, &short_packet, sizeof(short_packet)));
            fsync(bulkin_fd);

            if ( status != sizeof(short_packet) )
            {
                localErrno = errno;
                usbtmc488_tsprintf("%s: short packet write error: wrote %d of %d bytes\n"
                           "  localErrno=%d (%s)\n",
                    __FUNCTION__,
                    status,
                    sizeof(short_packet),
                    localErrno,
                    strerror(localErrno));
                bulkin_inprogress = BULKIN_IDLE;
                bulkin_state = BULK_EP_IDLE;
                UNLOCK(BULKIN_LOCK);
                responseState = IDLE;
                UNLOCK(RESPONSE_LOCK);
                return(USBTMC488_FAILED);
            }
        }
        UNLOCK(BULKIN_LOCK);
        UNLOCK(RESPONSE_LOCK);
    }

    WAITFOR(BULKIN_LOCK);
    bulkin_inprogress = BULKIN_IDLE;
    bulkin_state = BULK_EP_IDLE;
    UNLOCK(BULKIN_LOCK);
    UNLOCK(RESPONSE_LOCK);
    return(retStatus);
}

PRIVATE void
usbtmc_intrpt_thread_cleanup(void* dummy)
{
    int sem_status = 0, sem_errno = 0;

    (void)dummy;

    if ( 0 <= intr_fd )
    {
        close_fd(&intr_fd);
    }
    intr_tid = (pthread_t) -1;
    UNLOCK(INTRPT_LOCK);
    UNLOCK(STATUS_BYTE_LOCK);
    UNLOCK(BULKIN_LOCK);

    usbtmcDbgPrint(USBTMC488_THREADS,
        "%s: done\n", __FUNCTION__);
}

/* This thread for writing to the interrupt-in endpoint is implemented
 * because some hosts may not mechanize that endpoint and, therefore,
 * will not send IN tokens on that endpoint. In such cases, attempting
 * to write to this endpoint will hang waiting for an IN token. This
 * thread encapsulates writing to the interrupt-in endpoint and will
 * hang when no IN tokens are sent by the host but still allow the rest
 * of the USBTMC interface to function normally. The thread is driven
 * by the INTRPT_EVENT and signals the INTRPT_DONE_EVENT when the
 * write completes. Since the INTRPT_DONE_EVENT may never be signalled,
 * as is the case when the host is not sending IN tokens on the endpoint,
 * waiting for the INTRPT_DONE_EVENT is done with a timeout of 10 msec.
 */
PRIVATE void *
usbtmc_intrpt_thread( void *param )
{
    int         status;
    int         sem_status = 0, sem_errno = 0;
    int         localErrno = 0;

    (void)param;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    usbtmcThreadArray[USBTMC488_INTRPT_THREAD].pid = syscall(__NR_gettid);
#else /* LINUX_VERSION_CODE */
    usbtmcThreadArray[USBTMC488_INTRPT_THREAD].pid = getpid();
#endif /* LINUX_VERSION_CODE */
    strncpy(usbtmcThreadArray[USBTMC488_INTRPT_THREAD].name,
        "usbtmc intrpt thread",
            USBTMC_MAX_THREADNAME_LENGTH);

    usbtmcDbgPrint(USBTMC488_THREADS, "%s: ENTRY\n", __FUNCTION__);
    pthread_cleanup_push(usbtmc_intrpt_thread_cleanup, NULL);

    while ( TRUE == device_connected )
    {
        pthread_testcancel();
        WAITFOR(INTRPT_EVENT);
        if ( TRUE != device_connected )
        {
            usbtmcDbgPrint(USBTMC488_THREADS,
                "%s: device not connected\n", __FUNCTION__);
            /* Must be disconnecting */
            continue;
        }
        usbtmcDbgPrint(USBTMC488_EP_INTRPT,
            "%s: Writing intrpt response: bNotify1=%d, bNotify2=%d\n",
            __FUNCTION__, usbtmc_interrupt_in_resp.bNotify1,
            usbtmc_interrupt_in_resp.bNotify2);

        status = TEMP_FAILURE_RETRY(
             write(intr_fd,
                   &usbtmc_interrupt_in_resp,
                   sizeof(struct usbtmc_interrupt_in_resp))
             );
        fsync(intr_fd);

        usbtmcDbgPrint(USBTMC488_EP_INTRPT,
            "%s: Wrote intrpt response\n", __FUNCTION__);
        if ( status != sizeof(struct usbtmc_interrupt_in_resp) )
        {
            localErrno = errno;
            if ( ESHUTDOWN != localErrno )
            {
                usbtmc488_tsprintf("%s: Error writing interrupt_in packet\n"
                        "localErrno=%d (%s)\n",
                        __FUNCTION__, localErrno, strerror(localErrno));
            }
        }
        SIGNAL_EVENT(INTRPT_DONE_EVENT);
    }
    usbtmcDbgPrint(USBTMC488_THREADS, "%s: EXIT\n", __FUNCTION__);
    pthread_cleanup_pop(1);
    return(0);
}

PRIVATE void
usbtmc_request_thread_cleanup(void* dummy)
{
    int sem_status = 0, sem_errno = 0;

    (void)dummy;

    close_fd(&bulkout_fd);

    /* Under normal circumstances, the request thread should terminate
     * upon disconnect by virtue of the fact that the kernel layer should
     * terminate the blocked read with an error. We have found that this
     * does not occur with some implementations and, therefore, added the
     * provision below to unlock these locks.
     */
    UNLOCK(BULKOUT_LOCK);
    UNLOCK(REQUEST_LOCK);
    usbtmcDbgPrint(USBTMC488_THREADS, "%s: done\n", __FUNCTION__);
}

PRIVATE void *
usbtmc_request_thread(void *param)
{
    char                            *name = (char *) param;
    int                              sem_status    = 0;
    int                              sem_errno     = 0;
    int                              readStatus    = 0;
    long int                         localErrno    = 0;
    __u32                            padBytes      = 0;
    __u32                            bytesToRead   = 0;
    int                              bThreadDone   = FALSE;
    struct usbtmc_bulk_out_msg      *msg           =
            (struct usbtmc_bulk_out_msg *)pRequestBuffer;
    const __u32                      hdrSize       = sizeof(struct usbtmc_bulk_out_msg);
    __u32                            transferSize, transferBytesRead;
    __u8                             bTransferAttribute;
    USBTMC488_MESSAGE                eventMsg;

    usbtmcDbgPrint(USBTMC488_THREADS, "%s: name = %s\n", __FUNCTION__, name );

    /* The bulkout endpoint should have been opened at this point and should
     * have a valid file descriptor.
     */
    if ( bulkout_fd < 0 )
    {
        usbtmc488_tsprintf("%s: endpoint %s open failed\n", __FUNCTION__, name);
        return(0);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    usbtmcThreadArray[USBTMC488_BULKOUT_THREAD].pid = syscall(__NR_gettid);
#else /* LINUX_VERSION_CODE */
    usbtmcThreadArray[USBTMC488_BULKOUT_THREAD].pid = getpid();
#endif /* LINUX_VERSION_CODE */
    strncpy(usbtmcThreadArray[USBTMC488_BULKOUT_THREAD].name,
        "usbtmc bulkout thread",
            USBTMC_MAX_THREADNAME_LENGTH);

    pthread_cleanup_push(usbtmc_request_thread_cleanup, NULL);
    do
    {
        pthread_testcancel();
        errno = localErrno = 0;

        WAITFOR(REQUEST_LOCK);
        transferSize = 0;
        transferBytesRead = 0;

        /* Read the message header and as much data as we can take.
         *
         * NOTE: Ideally, we would request only 12 bytes (the size of the
         *       message header) on the first read to obtain the message
         *       header and, if the message ID (bMsgID field in the
         *       usbtmc_bulk_out_hdr struct) is DEV_DEP_MSG_OUT, then based on
         *       the transfer size (lTransferSize in the usbtmc_bulk_out_msg
         *       struct), continue reading and processing the data until
         *       lTransferSize bytes of payload data have been read and processed.
         *
         *       We have found that this is not possible with some kernel
         *       driver implementations. For example, the AMCC DWC-OTG
         *       driver returns 20 bytes when 12 bytes is requested.
         *
         *       We have also found that the kernel layer does not get an
         *       interrupt to indicate that a bulk message is available
         *       for certain bulkout read request lengths. This problem occurs for
         *       any transfer size (FS and HS) where the difference between integral
         *       multiples of max packet size (64 for FS and 512 for HS) and the
         *       transfer size is 12 through 15 (header size + up to 3 pad bytes).
         *       For example:
         *
         *	 Full Speed Failed Transfer Sizes
         *         o    49 through 52 bytes (64 * 1 - 49 = 15) through (64 * 1 - 52 = 12)
         *         o    113 through 116 bytes (64 * 2 - 113 = 15) through (64 * 2 - 116 = 12)
         *         o    177 through 180 bytes (64 * 3 - 177 = 15) through (64 * 3 - 180 = 12)
         *         o    etc
         *       High Speed Failed Transfer Sizes
         *         o    497 through 500 bytes (512 * 1 - 497 = 15) through (512 * 1 - 500 = 12)
         *         o    1009 through 1012 bytes (512 * 2 - 1009 = 15) through (512 * 2 - 1012 = 12)
         *         o    1521 through 1524 bytes (512 * 3 - 1521 = 15) through (512 * 3 - 1524 = 12)
         *         o    etc
         *
         *       To resolve this, we request only the maximum packet size (64 or 512) bytes
         *       for the first read.
         */
        bytesToRead = MaxPacketSize;

        WAITFOR(BULKOUT_LOCK);

//sjz        printf("CALLING READ ON bulkout_fd\n");
        readStatus = BULK_FAILURE_RETRY(read(bulkout_fd,
                            pRequestBuffer,
                            bytesToRead));

//sjz        printf("BACK FROM READ ON bulkout_fd\n");
        localErrno = errno;
        UNLOCK(BULKOUT_LOCK);
        usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
            "Read %d bytes, Requested %d bytes - LINE %d\n",
            readStatus, bytesToRead, __LINE__);

        /* MaxTransferSize is set to 0 at the end of stop_io(). This condition means
         * it's safe to terminate the request thread.
         */
        if ( 0 == MaxTransferSize )
        {
            usbtmc488_tsprintf("%s: request_thread terminated: readStatus=%d\n",
                __FUNCTION__, readStatus);
            UNLOCK(REQUEST_LOCK);
            bThreadDone = TRUE;
            continue;
        }

        bulkout_state = BULK_EP_INPROGRESS;
        if ( readStatus < 0 )
        {
            if ( ESHUTDOWN == localErrno )
            {
                usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                    "%s: got ESHUTDOWN reading bulkout_fd\n", __FUNCTION__);
                continue;
            }
            else
            {
                usbtmc488_tsprintf(
                    "usbtmc488: %s: read error reading header - localErrno=%d (%s)\n"
                    "  MaxTransferSize=%d, readStatus=%d\n",
                    __FUNCTION__,
                    localErrno,
                    strerror(localErrno),
                    MaxTransferSize,
                    readStatus);
                goto halt_bulkout;
            }
        }

        transferSize = __le32_to_cpu(msg->lTransferSize);
        bTransferAttribute = msg->bmTransfer;

        /* Any message other than DEV_DEP_MSG_OUT should have
         * no data beyond the header.
         */
        if ( msg->sHdr.bMsgID == DEV_DEP_MSG_OUT )
        {
            padBytes = (ALIGNMENT - ((transferSize+hdrSize)&(ALIGNMENT-1)));
            if ( padBytes == ALIGNMENT )
            {
                padBytes = 0;
            }
            usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                "DEV_DEP_MSG_OUT transferSize=%d, padBytes=%d at LINE %d\n",
                transferSize, padBytes, __LINE__);
            if ( readStatus >= (int)(transferSize+hdrSize) )
            {
                process_dev_dep_msg_in((struct usbtmc_bulk_in_msg *) pRequestBuffer);
                if ( readStatus >= (int)(transferSize+hdrSize+padBytes) )
                {
                    if ( padBytes > 0 )
                    {
                        usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                            "Consumed %d PAD bytes at LINE %d\n",
                            padBytes, __LINE__);
                    }
                    padBytes = 0;
                }
            }
            else
            {
                transferBytesRead = readStatus - hdrSize;
                msg->lTransferSize = __cpu_to_le32(transferBytesRead);
                if ( transferBytesRead == transferSize )
                {
                    msg->bmTransfer = bTransferAttribute;
                }
                else
                {
                    msg->bmTransfer = 0;
                }
                process_dev_dep_msg_in((struct usbtmc_bulk_in_msg *) pRequestBuffer);

                while ( transferBytesRead < transferSize )
                {
                    bytesToRead = min(transferSize-transferBytesRead, MaxTransferSize);
                    msg->lTransferSize = __cpu_to_le32(bytesToRead);
                    /* We must read a multiple of 4 bytes. Read the pad bytes with the
                     * last payload read if necessary.
                     */
                    if ( (bytesToRead+padBytes) <= MaxTransferSize )
                    {
                        if ( padBytes > 0 )
                        {
                            usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                                "Consuming %d pad bytes with last %d "
                                "bytes of payload - LINE %d\n",
                                padBytes, bytesToRead, __LINE__);
                        }
                        bytesToRead += padBytes;
                        padBytes = 0;
                    }
                    WAITFOR(BULKOUT_LOCK);
                    readStatus = BULK_FAILURE_RETRY(read(bulkout_fd,
                                    pRequestBuffer + hdrSize,
                                    bytesToRead));
                    localErrno = errno;
                    UNLOCK(BULKOUT_LOCK);
                    usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                        "Read %d bytes, Requested %d bytes, "
                        "padBytes=%d - LINE %d\n",
                        readStatus, bytesToRead, padBytes, __LINE__);
                    if ( readStatus < (int)bytesToRead )
                    {
                        usbtmc488_tsprintf(
                            "usbtmc488: %s: read error reading payload - "
                            "localErrno=%d (%s)\n"
                            "bytesToRead=%d, readStatus=%d\n",
                            __FUNCTION__,
                            localErrno,
                            strerror(localErrno),
                            bytesToRead,
                            readStatus);
                        goto halt_bulkout;
                    }
                    transferBytesRead += bytesToRead;
                    if ( transferBytesRead >= transferSize )
                    {
                        msg->bmTransfer = bTransferAttribute;
                    }
                    else
                    {
                        msg->bmTransfer = 0;
                    }
                    process_dev_dep_msg_in((struct usbtmc_bulk_in_msg *) pRequestBuffer);
                }
                if ( readStatus >= (int)(transferSize+hdrSize+padBytes) )
                {
                    usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                        "Consumed %d PAD bytes at LINE %d\n",
                        padBytes, __LINE__);
                    padBytes = 0;
                }
            }
            if ( padBytes > 0 )
            {
                /* We must request at least 4 bytes from the kernel. We shouldn't ever
                 * get here because the pad bytes should have been delivered with the
                 * previous reads.
                 */
                usbtmc488_tsprintf("%s: Unexpected %d remaining pad bytes - %s:%d\n"
                    "Attempting to read %d bytes\n",
                    __FUNCTION__, padBytes, __FILE__, __LINE__, ALIGNMENT);
                padBytes = ALIGNMENT;
                WAITFOR(BULKOUT_LOCK);
                readStatus = BULK_FAILURE_RETRY(read(bulkout_fd,
                                    pRequestBuffer + hdrSize,
                                    padBytes));
                localErrno = errno;
                UNLOCK(BULKOUT_LOCK);
                usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                    "Read %d bytes, Requested %d PAD bytes - LINE %d\n",
                    readStatus, padBytes, __LINE__);
                if ( readStatus < (int)padBytes )
                {
                    usbtmc488_tsprintf(
                        "usbtmc488: %s: read error reading %d pad bytes - "
                        "localErrno=%d (%s)\n"
                        "readStatus=%d\n",
                        __FUNCTION__,
                        padBytes,
                        localErrno,
                        strerror(localErrno),
                        readStatus);
                    goto halt_bulkout;
                }
            }
        }

        switch ( msg->sHdr.bMsgID )
        {
            case DEV_DEP_MSG_OUT:
                usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                    "%s: DEV_DEP_MSG_OUT handled\n", __FUNCTION__);
                break;
            case REQUEST_DEV_DEP_MSG_IN:
                usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                    "%s: REQUEST_DEV_DEP_MSG_IN: transferSize=%d\n",
                    __FUNCTION__, transferSize);
                switch ( bulkin_inprogress )
                {
                    case BULKIN_IDLE:
                        process_dev_dep_msg_in(
                            (struct usbtmc_bulk_in_msg *) pRequestBuffer);
                        break;
                    case BULKIN_HALT:
                        UNLOCK(REQUEST_LOCK);
                        break;
                    case BULKIN_INPROGRESS:
                        if ( TERMINATE == responseState )
                        {
                            WAITFOR(BULKIN_LOCK);
                            bulkin_inprogress = BULKIN_IDLE;
                            bulkin_state = BULK_EP_IDLE;
                            UNLOCK(BULKIN_LOCK);
                        }
                        else if ( IN_PROGRESS == responseState )
                        {
                            /* Host is reading response incrementally */
                            usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                                "%s: bulkin_inprogress=%d, responseState=%d\n",
                                __FUNCTION__, bulkin_inprogress, responseState);
                            process_dev_dep_msg_in(
                                (struct usbtmc_bulk_in_msg *) pRequestBuffer);
                            break;
                        }
                        else if ( IDLE == responseState )
                        {
                            /* Response must have been flushed */
                            usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                                "%s: bulkin_inprogress=%d, responseState=%d\n",
                                __FUNCTION__, bulkin_inprogress, responseState);
                            process_dev_dep_msg_in(
                                (struct usbtmc_bulk_in_msg *) pRequestBuffer);
                            break;
                        }
                        else
                        {
                            usbtmc488_tsprintf("%s: HALT BULKIN; responseState = %d\n",
                                __FUNCTION__, responseState);
                            goto halt_bulkin;
                        }
                        break;
                    default:
                        usbtmc488_tsprintf("%s: Unexpected bulkin_inprogress = %d\n",
                            __FUNCTION__, bulkin_inprogress);
                        goto halt_bulkin;
                        break;
                }
                break;
            case VENDOR_SPECIFIC_OUT:
                usbtmc488_tsprintf("usbtmc488: %s: Unsupported MsgID: "
                    "VENDOR_SPECIFIC_OUT\n", __FUNCTION__);
                goto halt_bulkout;
                break;
            case REQUEST_VENDOR_SPECIFIC_IN:
                usbtmc488_tsprintf("usbtmc488: %s: Unsupported MsgID: "
                    "REQUEST_VENDOR_SPECIFIC_IN\n", __FUNCTION__);
                goto halt_bulkout;
                break;
            case USB488_TRIGGER:
                usbtmcDbgPrint(USBTMC488_EP_BULKOUT,
                    "%s: USB488_TRIGGER: %d\n",
                    __FUNCTION__, transferSize);
                eventMsg.type   = USBTMC488_MSG_DEVICE_TRIGGER;
                eventMsg.value = 0;
                eventMsg.msg_buffer = NULL;
                usbtmc_event_handler(&eventMsg, pData);
                break;
            default:
                usbtmc488_tsprintf("usbtmc488: %s: Unsupported MsgID = %d\n",
                    __FUNCTION__, msg->sHdr.bMsgID);
                goto halt_bulkout;
                break;
        }

        bulkout_state = BULK_EP_IDLE;

        if ( (readStatus < 0) && (localErrno != ESHUTDOWN) )
        {
            usbtmc488_tsprintf(
                "usbtmc488: %s: abnormal termination - localErrno=%d (%s)\n",
                __FUNCTION__,
                localErrno,
                strerror(localErrno));
            UNLOCK(REQUEST_LOCK);
            break;
        }
        continue;
    halt_bulkin:
        usbtmc488_tsprintf("%s: HALT BULKIN\n", __FUNCTION__);
        /* halt/stall the Bulk-In Endpoint */
        bulkin_inprogress = BULKIN_HALT;
        bulkin_state = BULK_EP_HALT;
        UNLOCK(REQUEST_LOCK);
        continue;
    halt_bulkout:
        /* halt/stall the Bulk-Out Endpoint */
        /* Host must send a CLEAR_FEATURE to clear this condition */
        usbtmc488_tsprintf("%s: HALT BULKOUT\n", __FUNCTION__);
        bulkout_state = BULK_EP_HALT;
        UNLOCK(REQUEST_LOCK);
        continue;
    } while ( ! bThreadDone );

    fflush(stdout);
    pthread_cleanup_pop(1);

    UNLOCK(REQUEST_LOCK);
    return(0);
}

PRIVATE void *(*bulkout_thread) (void *);
PRIVATE void *(*intrpt_thread) (void *);


PRIVATE int
start_io()
{
    int            sem_status = 0,
               sem_errno  = 0,
               localErrno = 0;
    int            status;
    pthread_attr_t attr;

    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s: ENTRY\n", __FUNCTION__);

    /* Force the device clear lock to the locked state */
    UNLOCK(DEVICE_CLEAR_LOCK);
    WAITFOR(DEVICE_CLEAR_LOCK);
    UNSIGNAL_EVENT(INTRPT_EVENT);

    bulkin_state = bulkout_state = BULK_EP_IDLE;

    if ( device_connected )
    {
        usbtmc488_tsprintf("usbtmc488: %s: IO already started\n",
            __FUNCTION__);
        return(0);
    }
    if ( FALSE == usbtmc_interface_enabled )
    {
        usbtmc488_tsprintf("usbtmc488: %s: interface not enabled\n",
            __FUNCTION__);
        return(0);
    }

    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s: Starting I/O\n", __FUNCTION__);

    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s: Start bulk & intrpt EPs\n", __FUNCTION__);

    bulkin_fd = bulkin_open(EP_IN_NAME);
    localErrno = errno;
    if (bulkin_fd < 0)
    {
        usbtmc488_tsprintf(
            "usbtmc488: %s: can't open bulkin endpoint: localErrno=%d (%s)\n"
            "EP_IN_NAME = '%s'\n",
            __FUNCTION__, localErrno, strerror(localErrno), EP_IN_NAME);
        return(-1);
    }
    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s:\n"
        "BULK-IN\n"
        "    Name:       %s\n"
        "    File Descr: %d\n",
        __FUNCTION__, EP_IN_NAME, bulkin_fd);

    bulkout_fd = bulkout_open(EP_OUT_NAME);
    localErrno = errno;
    if (bulkout_fd < 0)
    {
        usbtmc488_tsprintf(
            "usbtmc488: %s: can't open bulkout endpoint: localErrno=%d (%s)\n"
            "EP_OUT_NAME = '%s'\n",
            __FUNCTION__, localErrno, strerror(localErrno), EP_OUT_NAME);
        return(-1);
    }
    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s:\n"
        "BULK-OUT\n"
        "    Name:       %s\n"
        "    File Descr: %d\n",
        __FUNCTION__, EP_OUT_NAME, bulkout_fd);

    intr_fd = intr_open(EP_INTR_NAME);
    localErrno = errno;
    if (intr_fd < 0)
    {
        usbtmc488_tsprintf(
            "usbtmc488: %s: can't open intrpt endpoint: localErrno=%d (%s)\n"
            "EP_INTR_NAME = '%s'\n",
            __FUNCTION__, localErrno, strerror(localErrno), EP_INTR_NAME);
        return(-1);
    }
    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s:\n"
        "INTR-IN\n"
        "    Name:       %s\n"
        "    File Descr: %d\n",
        __FUNCTION__, EP_INTR_NAME, intr_fd);

    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s: create bulkout thread\n", __FUNCTION__);
    status = pthread_attr_init(&attr);
    if ( 0 != status )
    {
        usbtmc488_tsprintf("%s: pthread_attr_init failed: error=%d (%s)\n",
            __FUNCTION__, status, strerror(status));
        return(-1);
    }
    status = pthread_attr_setstacksize(&attr, USBTMC488_STACK_SIZE);
    if ( 0 != status )
    {
        usbtmc488_tsprintf("%s: pthread_attr_setstacksize failed: error=%d (%s)\n",
            __FUNCTION__, status, strerror(status));
        return(-1);
    }

    if ( pthread_create(&bulkout_tid, &attr,
            bulkout_thread, (void *) EP_OUT_NAME) != 0 )
    {
        perror("usbtmc488: can't create bulkout thread - error info");
    }

    pthread_setname_np(bulkout_tid, "USBTMCblkout");

    /* device_connected is used by the intrpt_thread to determine when to
     * exit.
     */
    device_connected = TRUE;
    if ( pthread_create(&intr_tid, &attr,
            intrpt_thread, (void *) NULL) != 0 )
    {
        perror("usbtmc488: can't create intrpt thread - error info");
    }

    pthread_setname_np(intr_tid, "USBTMCintr");

    status = pthread_attr_destroy(&attr);
    if ( 0 != status )
    {
        usbtmc488_tsprintf("%s: pthread_attr_destroy failed: error=%d (%s)\n",
            __FUNCTION__, status, strerror(status));
    }
    usbtmcDbgPrint(USBTMC488_SYSTEM,
        "%s: EXIT\n", __FUNCTION__);
    return(0);
}

PRIVATE void
stop_io()
{
    int sem_status = 0,
        sem_errno  = 0;
    int status     = 0;

    usbtmcDbgPrint(USBTMC488_SYSTEM, "%s: ENTRY\n", __FUNCTION__);
    /* Force the device clear lock to the locked state */
    UNLOCK(DEVICE_CLEAR_LOCK);
    WAITFOR(DEVICE_CLEAR_LOCK);

    if ( 0 <= bulkin_fd )
    {
        close_fd(&bulkin_fd);
    }
    device_connected = FALSE;
    SIGNAL_EVENT(INTRPT_EVENT);
    /* Allow time for the interrupt thread to exit. */
    usbtmc488_sleep(100);
    WAITFOR(RESPONSE_LOCK);
    responseState = IDLE;
    bulkin_inprogress = BULKIN_IDLE;
    bulkin_state = bulkout_state = BULK_EP_IDLE;
    UNLOCK(RESPONSE_LOCK);

    if ( (pthread_t)-1 != bulkout_tid )
    {
        status = pthread_cancel(bulkout_tid);
        if ( (0 != status) && (status != ESRCH) )
        {
            usbtmc488_tsprintf("%s: error %d (%s) cancelling bulkout thread\n",
                    __FUNCTION__, status, strerror(status));
        }
        status = pthread_join(bulkout_tid, NULL);
        if ( 0 != status )
        {
            usbtmc488_tsprintf("%s: error returned from pthread_join(bulkout_tid): %d (%s)\n",
                    __FUNCTION__, status, strerror(status));
        }
        bulkout_tid = (pthread_t) -1;
    }
    MaxTransferSize = 0;
    MaxPacketSize = 0;
    usbtmcDbgPrint(USBTMC488_SYSTEM, "%s: EXIT\n", __FUNCTION__);
}

/*-------------------------------------------------------------------------*/

PRIVATE int
init_device(void)
{
    int             localErrno = 0;
    char            buf [4096], *cp = &buf [0];
    int             status;

    status = autoconfig();

    if ( status < 0 )
    {
        usbtmc488_tsprintf(
                "usbtmc488: %s: No recognized device found\n", __FUNCTION__);
        return(status);
    }

    ep0_fd = open(EP0_NAME, O_RDWR);
    localErrno = errno;

    if ( ep0_fd < 0 )
    {
        usbtmc488_tsprintf("usbtmc488: %s: Unable to open %s\n"
                "  localErrno=%d (%s)\n",
                __FUNCTION__, EP0_NAME, localErrno, strerror(localErrno));
        return(-localErrno);
    }

    usbtmcDbgPrint(USBTMC488_SYSTEM,
            "%s:\n"
            "EP0\n"
            "    Name:       %s\n"
            "    File Descr: %d\n",
            __FUNCTION__, EP0_NAME, ep0_fd);

    status = TEMP_FAILURE_RETRY(write(ep0_fd, &descriptors, sizeof descriptors));
    fsync(ep0_fd);

    if ( status < 0 )
    {
        perror("usbtmc488: write dev descriptors - error info");
        close(ep0_fd);
        ep0_fd = -ENODEV;
        return(status);
    }
    else if ( status != sizeof descriptors )
    {
        usbtmc488_tsprintf("usbtmc488: %s: dev init, wrote %d expected %d\n",
                __FUNCTION__, status, cp - buf);
        close(ep0_fd);
        ep0_fd = -ENODEV;
        return(-EIO);
    }

    status = TEMP_FAILURE_RETRY(write(ep0_fd, strings, strings->header.length));
    fsync(ep0_fd);

    if ( status < 0 )
    {
        perror("usbtmc488: write dev strings - error info");
        close(ep0_fd);
        ep0_fd = -ENODEV;
        return(status);
    }
    else if ( status != (int) strings->header.length )
    {
        usbtmc488_tsprintf("usbtmc488: %s: dev init, wrote %d expected %d\n",
                __FUNCTION__, status, cp - buf);
        close(ep0_fd);
        ep0_fd = -ENODEV;
        return(-EIO);
    }

    return(ep0_fd);
}

PRIVATE int
handle_tmc_control(int fd, struct usb_ctrlrequest *setup)
{
//sjz printf("handle_tmc_control fd %d\n", fd);
    int                        sem_status          = 0,
                               sem_errno           = 0;
    int                        localErrno          = 0;
    int                        status, resp_length = 0;
    int                        fifo_status         = 0;
    int                        timed_wait_status   = 0;
    void                       *buf = NULL;
    __u16                      value, index, length;
    __u8                       bTag_setup;
    __u8                       bTag_last_bulkin, bTag_last_bulkout;
    struct usbtmc_bulk_in_msg  short_bulkin_packet;
    USBTMC488_MESSAGE          controlMsg;

    value = __le16_to_cpu(setup->wValue);
    index = __le16_to_cpu(setup->wIndex);
    length = __le16_to_cpu(setup->wLength);

    bTag_setup = (__u8)value;

    WAITFOR(RESPONSE_LOCK);
    bTag_last_bulkin  = last_req_dev_dep_msgin_msg.sHdr.bTag;
    bTag_last_bulkout = last_bulkout_hdr.bTag;
    UNLOCK(RESPONSE_LOCK);

    memset((__u8 *)&usbtmc_short_resp, 0, sizeof(usbtmc_short_resp));

    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
        "%s: %s:%02x-%02x.%02x "
        "value=%04x index=%04x length=%d\n",
        __FUNCTION__,
        setup->bRequestType & USB_DIR_IN ? "IN" : "OUT",
        setup->bRequestType & USB_TYPE_MASK,
        setup->bRequestType & ~(USB_DIR_IN|USB_TYPE_MASK),
        setup->bRequest, value, index, length);

    switch ( setup->bRequest )
    {
        case USBTMC_REQ_GET_CAPABILITIES:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: GET_CAPABILITIES\n", __FUNCTION__);
            buf = &usbtmc_get_cap_resp;
            resp_length = sizeof(usbtmc_get_cap_resp);
            break;

        case USBTMC_REQ_INITIATE_ABORT_BULK_OUT:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INITIATE_ABORT_BULK_OUT\n", __FUNCTION__);

            /* Abort Bulk-OUT transfer, halt Bulk-out endpoint if a
             * a transfer is inprogress and respond with:
             *
             * STATUS_SUCCESS if the specified transfer is in progress.
             *      halt endpoint.
             *
             * STATUS_TRANSFER_NOT_IN_PROGRESS if there is no transfer
             *      inprogress and the "FIFO"(input) is not empty, or
             *      the bTag does not match.  halt endpoint.
             *
             * STATUS_FAILED if there is no transfer in progress and "FIFO"
             *      is empty.  do not halt endpoint.
             */
#if (FUNCTIONFS_FIFO_STATUS_IMPLEMENTED)
            WAITFOR(BULKOUT_LOCK);
            fifo_status = ioctl(bulkout_fd, FUNCTIONFS_FIFO_STATUS);
            localErrno = errno;
            UNLOCK(BULKOUT_LOCK);
            if ( (fifo_status < 0) && (localErrno != EOPNOTSUPP) && (localErrno != ESHUTDOWN) )
            {
                usbtmc488_tsprintf("%s: FIFO_STATUS ioctl failure: fifo_status=%d\n"
                           "  localErrno=%d (%s)\n",
                    __FUNCTION__, fifo_status, localErrno, strerror(localErrno));
                fifo_status = 0;
            }
#endif /* FUNCTIONFS_FIFO_STATUS_IMPLEMENTED */
            switch ( bulkout_state )
            {
            case BULK_EP_INPROGRESS:
                    if ( bTag_last_bulkout == bTag_setup )
                    {
                        usbtmc_short_resp.bStatus = STATUS_SUCCESS;
                        /* Build and send a device clear message to the
                         * application and wait for the application to
                         * complete the device clear processing.
                         */
                        controlMsg.type = USBTMC488_MSG_DEVICE_CLEAR;
                        controlMsg.value = 0;
                        controlMsg.msg_buffer   = NULL;
                        usbtmc_event_handler(&controlMsg, pData);

                        /* Wait for the application to indicate that it's
                         * done with device clear processing. If the application fails
                         * to respond within DEVICE_CLEAR_DONE_TIMEOUT, continue as if
                         * it had responded.
                         */
                        timed_wait_status = timed_wait(DEVICE_CLEAR_LOCK,
                                           DEVICE_CLEAR_DONE_TIMEOUT);
                        if ( (0 != timed_wait_status) && (ETIMEDOUT == errno) )
                        {
                            usbtmc488_tsprintf("%s: Timeout (%d msec) waiting for "
                                "DEVICE_CLEAR_LOCK\n",
                                __FUNCTION__, DEVICE_CLEAR_DONE_TIMEOUT);
                            UNLOCK(DEVICE_CLEAR_LOCK);
                        }

#if (FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED)
                        if ( 0 < fifo_status )
                        {
                            fifo_status =
                                ioctl(fd, FUNCTIONFS_FIFO_FLUSH);
                            if ( (fifo_status < 0) &&
                                (errno != EOPNOTSUPP) )
                            {
                                perror("usbtmc488: fifo flush ioctl error info");
                            }
                        }
#endif /* FUNCTIONFS_FIFO_FLUSH_IMPLEMENTED */
                    }
                    else
                    {
                        usbtmc_short_resp.bStatus =
                            STATUS_TRANSFER_NOT_IN_PROGRESS;
                    }
                    break;
                case BULK_EP_IDLE:
                    usbtmc_short_resp.bStatus = STATUS_SUCCESS;
                    break;
                case BULK_EP_HALT:
                case BULK_EP_ERROR:
                    if ( fifo_status > 0 )
                    {
                        usbtmc_short_resp.bStatus =
                            STATUS_TRANSFER_NOT_IN_PROGRESS;
                    }
                    else
                    {
                        usbtmc_short_resp.bStatus = STATUS_FAILED;
                    }
                    break;
                default:
                    usbtmc_short_resp.bStatus = STATUS_FAILED;
                    break;
            }
            usbtmc_short_resp.bData = bTag_last_bulkout;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp);
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INITIATE_ABORT_BULK_OUT:\n"
                "  bulkout_state = %d\n"
                "  bStatus       = %d\n"
                "  bData         = %d\n",
                __FUNCTION__, bulkout_state, usbtmc_short_resp.bStatus, usbtmc_short_resp.bData);
            break;

        case USBTMC_REQ_CHECK_ABORT_BULK_OUT_STATUS:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: CHECK_ABORT_BULK_OUT_STATUS\n", __FUNCTION__);
            /* Check the status of the Bulk-Out endpoint after an ABORT,
             * respond with:
             *
             * STATUS_PENDING if the ABORT is not complete NBYTES_RXD
             *      not calculated (return zero?)
             *
             * STATUS_SUCCESS ABORT complete, NBYTES_RXD set to, the total
             *      number of USBTMC message data bytes (not including
             *      Bulk-OUT header or alignement bytes) in the transfer
             *      received, and not discarded, by the device.
             */
            usbtmc_check_abort_resp.bStatus = STATUS_SUCCESS;
            usbtmc_check_abort_resp.lNbytesXD = __cpu_to_le32(0);
            buf = &usbtmc_check_abort_resp;
            resp_length = sizeof(usbtmc_check_abort_resp);
            break;

        case USBTMC_REQ_INITIATE_ABORT_BULK_IN:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INITIATE_ABORT_BULK_IN\n", __FUNCTION__);

            /* Abort a Bulk-In transfer and/or resync USBTMC Bulk-In
             * transfers. Respond with:
             *
             * STATUS_SUCCESS if the specified transfer is in progress
             *      after the response is queued queue a short packet
             *      on the Bulk-In endpoint.
             *
             * STATUS_TRANSFER_NOT_IN_PROGRESS if the transfer in progress
             *      does not match, or there is no transfer in progress
             *      but the Bulk-Out FIFO is not empty.
             *
             * STATUS_FAILED if there is not a transfer in progress and
             *      the Bulk-Out FIFO is empty.
             *
             */
            usbtmc_short_resp.bData = bTag_last_bulkin;

#if (FUNCTIONFS_FIFO_STATUS_IMPLEMENTED)
            WAITFOR(BULKOUT_LOCK);
            fifo_status = ioctl(bulkout_fd, FUNCTIONFS_FIFO_STATUS);
            localErrno = errno;
            UNLOCK(BULKOUT_LOCK);
            if ( (fifo_status < 0) && (errno != EOPNOTSUPP) )
            {
                usbtmc488_tsprintf("%s: FIFO_STATUS ioctl failure: fifo_status=%d\n"
                           "  localErrno=%d (%s)\n",
                    __FUNCTION__, fifo_status, localErrno, strerror(localErrno));
                fifo_status = 0;
            }
#endif /* FUNCTIONFS_FIFO_STATUS_IMPLEMENTED */
            WAITFOR(RESPONSE_LOCK);
            responseState = TERMINATE;
            SIGNAL_EVENT(TALKADDR_OR_TERMINATE);
            UNLOCK(RESPONSE_LOCK);
            switch ( bulkin_state )
            {
                case BULK_EP_INPROGRESS:
                case BULK_EP_IDLE:
                    if ( (BULK_EP_INPROGRESS == bulkin_state) &&
                         (bTag_last_bulkin == bTag_setup)
                       )
                    {
                        usbtmc_short_resp.bStatus = STATUS_SUCCESS;
                        /* Build and send a device clear message to the
                         * application and wait for the application to
                         * complete the device clear processing.
                         */
                        controlMsg.type = USBTMC488_MSG_DEVICE_CLEAR;
                        controlMsg.value = 0;
                        controlMsg.msg_buffer   = NULL;
                        usbtmc_event_handler(&controlMsg, pData);

                        /* Wait for the application to indicate that it's
                         * done with device clear processing. If the application fails
                         * to respond within DEVICE_CLEAR_DONE_TIMEOUT, continue as if
                         * it had responded.
                         */
                        timed_wait_status = timed_wait(DEVICE_CLEAR_LOCK,
                                           DEVICE_CLEAR_DONE_TIMEOUT);
                        if ( (0 != timed_wait_status) && (ETIMEDOUT == errno) )
                        {
                            usbtmc488_tsprintf("%s: Timeout (%d msec) waiting for "
                                "DEVICE_CLEAR_LOCK\n",
                                __FUNCTION__, DEVICE_CLEAR_DONE_TIMEOUT);
                            UNLOCK(DEVICE_CLEAR_LOCK);
                        }

                    }
                    else if ( BULK_EP_INPROGRESS == bulkin_state )
                    {
                        usbtmc_short_resp.bStatus =
                            STATUS_TRANSFER_NOT_IN_PROGRESS;
                    }
                    else
                    {
                        usbtmc_short_resp.bStatus = STATUS_FAILED;
                    }

                    /* Send the control endpoint response first */
                    status = TEMP_FAILURE_RETRY(
                        write(fd, &usbtmc_short_resp, sizeof(usbtmc_short_resp)));
                    fsync(fd);

                    localErrno = errno;
                    if ( status != sizeof(usbtmc_short_resp) )
                    {
                        usbtmc488_tsprintf(
                            "usbtmc488: %s: Error writing control endpoint "
                            "response for abort bulkin\n"
                            "localErrno=%d (%s) (%d != %d)\n",
                            __FUNCTION__, localErrno, strerror(localErrno), status, sizeof(usbtmc_short_resp));
                    }

                    /* If the control endpoint response was STATUS_SUCCESS, send a short
                     * packet on bulkin  to terminate the transfer, per table 26 of the
                     * USBTMC specification version 1.0.
                     */
                    if ( STATUS_SUCCESS == usbtmc_short_resp.bStatus )
                    {
                        /* Send a short packet on the bulkin endpoint
                         * to terminate any transfer which may have been in progress.
                         */
                        memset(&short_bulkin_packet, 0, sizeof(short_bulkin_packet));
                        WAITFOR(RESPONSE_LOCK);
                        short_bulkin_packet.sHdr.bTag = last_req_dev_dep_msgin_msg.sHdr.bTag;
                        short_bulkin_packet.sHdr.bTagInv = last_req_dev_dep_msgin_msg.sHdr.bTagInv;
                        UNLOCK(RESPONSE_LOCK);
                        short_bulkin_packet.sHdr.bMsgID = DEV_DEP_MSG_IN;

                        WAITFOR(BULKIN_LOCK);
                        status = BULK_FAILURE_RETRY(
                             write(bulkin_fd,
                                   &short_bulkin_packet,
                                   sizeof(short_bulkin_packet)));
                        fsync(bulkin_fd);

                        localErrno = errno;
                        UNLOCK(BULKIN_LOCK);
                        if ( status != sizeof(short_bulkin_packet) )
                        {
                            usbtmc488_tsprintf(
                            "%s: error writing bulkin short packet: "
                            "write returned %d\n"
                            "  localErrno=%d (%s)\n",
                            __FUNCTION__,
                            status,
                            localErrno,
                            strerror(localErrno));
                        }
                    }
                    break;
                case BULK_EP_HALT:
                case BULK_EP_ERROR:
                    buf = &usbtmc_short_resp;
                    resp_length = sizeof(usbtmc_short_resp);

                    if ( fifo_status > 0 )
                    {
                        usbtmc_short_resp.bStatus =
                            STATUS_TRANSFER_NOT_IN_PROGRESS;
                    }
                    else
                    {
                        usbtmc_short_resp.bStatus = STATUS_FAILED;
                    }
                    break;
                default:
                    buf = &usbtmc_short_resp;
                    resp_length = sizeof(usbtmc_short_resp);
                    usbtmc_short_resp.bData = 0;
                    usbtmc_short_resp.bStatus = STATUS_FAILED;
                    break;
            }
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INITIATE_ABORT_BULK_IN:\n"
                "  bulkin_state = %d\n"
                "  bStatus      = %d\n"
                "  bData        = %d\n",
                __FUNCTION__, bulkin_state, usbtmc_short_resp.bStatus, usbtmc_short_resp.bData);
            break;

        case USBTMC_REQ_CHECK_ABORT_BULK_IN_STATUS:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: CHECK_ABORT_BULK_IN_STATUS\n", __FUNCTION__);
            /* Check progress of Abort Bulk-In operation, respond with:
             *
             * STATUS_PENDING if the short packet has not been sent
             *      if there are 1 or more queued packets set
             *      bmAbortBulkIn.D0 = 1.  Set NBYTES_TXD to zero.
             *
             * STATUS_SUCCESS if the short packet has been sent, the
             *      Bulk-In FIFO is empty and we are ready to receive
             *      commands.
             *
             */
            usbtmc_check_abort_resp.bStatus = STATUS_SUCCESS;
            usbtmc_check_abort_resp.lNbytesXD = __cpu_to_le32(0);
            usbtmc_check_abort_resp.bmAbortBulkIn = 0;
            buf = &usbtmc_check_abort_resp;
            resp_length = sizeof(usbtmc_check_abort_resp);
            break;

        case USBTMC_REQ_INITIATE_CLEAR:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INITIATE_CLEAR\n", __FUNCTION__);
            usbtmc_short_resp.bStatus = STATUS_SUCCESS;
            usbtmc_short_resp.bData = 0;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp.bStatus);

            /* Build and send a device clear message to the application
             * and wait for the application to complete the device clear
             * processing.
             */
            controlMsg.type = USBTMC488_MSG_DEVICE_CLEAR;
            controlMsg.value = 0;
            controlMsg.msg_buffer   = NULL;
            usbtmc_event_handler(&controlMsg, pData);

            /* Wait for the application to indicate that it's
             * done with device clear processing. If the application fails
             * to respond within DEVICE_CLEAR_DONE_TIMEOUT, continue as if
             * it had responded.
             */
            timed_wait_status = timed_wait(DEVICE_CLEAR_LOCK,
                               DEVICE_CLEAR_DONE_TIMEOUT);
            if ( (0 != timed_wait_status) && (ETIMEDOUT == errno) )
            {
                usbtmc488_tsprintf("%s: Timeout (%d msec) waiting for "
                    "DEVICE_CLEAR_LOCK\n",
                    __FUNCTION__, DEVICE_CLEAR_DONE_TIMEOUT);
                UNLOCK(DEVICE_CLEAR_LOCK);
            }

            WAITFOR(RESPONSE_LOCK);
            responseState = TERMINATE;
            bulkin_inprogress = BULKIN_IDLE;
            bulkin_state = BULK_EP_IDLE;
            SIGNAL_EVENT(TALKADDR_OR_TERMINATE);
            UNLOCK(RESPONSE_LOCK);
            break;

        case USBTMC_REQ_CHECK_CLEAR_STATUS:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: CHECK_CLEAR_STATUS\n", __FUNCTION__);
            usbtmc_short_resp.bStatus = STATUS_SUCCESS;
            usbtmc_short_resp.bData = 0;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp);
            break;

        case USBTMC_REQ_INDICATOR_PULSE:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INDICATOR_PULSE\n", __FUNCTION__);
            usbtmc_short_resp.bStatus = STATUS_FAILED;
            usbtmc_short_resp.bData = 0;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp.bStatus);
            break;

        case USB488_REQ_READ_STATUS_BYTE:
            /* This request gets 2 responses:
             *    One on the interrupt-in endpoint
             *    One on the control endpoint
             *
             * Here's the relevent protocol requirement from the USB488 sub-class spec:
             *
             * When a device receives READ_STATUS_BYTE, and the USB488 interface has an
             * Interrupt-IN endpoint, the device must queue the control endpoint response
             * shown below in Table 13. In addition, the device must return a response on the
             * Interrupt-IN endpoint. The format of the response on the Interrupt-IN endpoint
             * is shown in Table 7. The device must queue the Interrupt-IN endpoint response
             * and then queue this control endpoint response. If the Interrupt-IN endpoint
             * response can not be queued because the Interrupt-IN FIFO is full, the device
             * must set the control endpoint response USBTMC_status = STATUS_INTERRUPT_IN_BUSY.
             *
             * Because some hosts are erroneously expecting the control endpoint response
             * to be sent before the interrupt-in response, we are purposely violating the
             * sequencing requirement above.
             */
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: READ_STATUS_BYTE\n", __FUNCTION__);
            /* Allow any bulkin transfers to complete because the kernel driver (dwc_otg)
             * shares a FIFO for bulkin and interrupt-in - sheesh!
             */
            WAITFOR(BULKIN_LOCK);
            WAITFOR(STATUS_BYTE_LOCK);
            WAITFOR(INTRPT_LOCK);
            usbtmc_interrupt_in_resp.bNotify2 = usbtmc_status_byte;
            usbtmc_read_sb_resp.bTag = bTag_setup; /* bTag of request */

            usbtmc_read_sb_resp.bStatusByte = 0; /* Device has an interrupt-in endpoint and
                                  * device status is reported there.
                                 */
            usbtmc_interrupt_in_resp.bNotify1 = bTag_setup;
            usbtmc_interrupt_in_resp.bNotify1 |= 0x80;
            usbtmc_read_sb_resp.bStatus = STATUS_SUCCESS;
            status = TEMP_FAILURE_RETRY(
                write(fd, &usbtmc_read_sb_resp, sizeof(struct usbtmc_read_sb_resp)));
            fsync(fd);

            localErrno = errno;
            if ( status != sizeof(struct usbtmc_read_sb_resp) )
            {
                usbtmc488_tsprintf(
                    "usbtmc488: %s: Error writing READ_STATUS_BYTE control endpoint "
                    "response\n"
                    "localErrno=%d (%s)\n",
                    __FUNCTION__, localErrno, strerror(localErrno));
                UNLOCK(INTRPT_LOCK);
                UNLOCK(STATUS_BYTE_LOCK);
                UNLOCK(BULKIN_LOCK);
                return(-1);
            }
            /* Signal the interrupt thread to write the interrupt-in response */
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: signal 1 INTRPT_EVENT\n", __FUNCTION__);
            SIGNAL_EVENT(INTRPT_EVENT);
            /* Wait for the interrupt thread to write the response or a timeout. If
             * the host does not send IN tokens on interrupt-in, then the INTRPT_DONE_EVENT
             * will remain locked and the interrupt thread will be stuck in it's
             * write to the interrupt-in file descriptor.
             */
            timed_wait_status = timed_wait(INTRPT_DONE_EVENT, INTRPT_TIMEOUT);
            if ( (0 != timed_wait_status) && (ETIMEDOUT == errno) )
            {
                usbtmc488_tsprintf("%s: READ_STATUS_BYTE Timeout (%d msec) waiting for "
                    "INTRPT_DONE_EVENT\n",
                    __FUNCTION__, INTRPT_TIMEOUT);
            }
            else if ( 0 != timed_wait_status )
            {
                usbtmc488_tsprintf("%s: Error %d (%s) waiting for INTRPT_DONE_EVENT\n",
                    __FUNCTION__, timed_wait_status, strerror(timed_wait_status));
            }
            UNLOCK(INTRPT_LOCK);
            UNLOCK(STATUS_BYTE_LOCK);
            UNLOCK(BULKIN_LOCK);

            /* Build and send a device clear message to the
             * application */
            controlMsg.type = USBTMC488_MSG_UPDATE_LOCAL_STATUS_BYTE;
            controlMsg.value = 0;
            controlMsg.msg_buffer   = NULL;
            usbtmc_event_handler(&controlMsg, pData);

            return(0);
            break;

        case USB488_REQ_REN_CONTROL:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: REN_CONTROL value %d\n", __FUNCTION__, value);
            usbtmc_short_resp.bStatus = STATUS_SUCCESS;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp.bStatus);

            /* Build and send a device clear message to the
             * application */
            if (bTag_setup == 0)
            {
                controlMsg.type = USBTMC488_MSG_REN_DISABLE;
            }
            else
            {
                controlMsg.type = USBTMC488_MSG_REN_ENABLE;                
            }
            controlMsg.value = 0;
            controlMsg.msg_buffer   = NULL;
            usbtmc_event_handler(&controlMsg, pData);

            break;

        case USB488_REQ_GO_TO_LOCAL:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: GO_TO_LOCAL\n", __FUNCTION__);
            usbtmc_short_resp.bStatus = STATUS_SUCCESS;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp.bStatus);

            /* Build and send a device clear message to the
             * application */
            controlMsg.type = USBTMC488_MSG_GOTO_LOCAL;
            controlMsg.value = 0;
            controlMsg.msg_buffer   = NULL;
            usbtmc_event_handler(&controlMsg, pData);

            break;

        case USB488_REQ_LOCAL_LOCKOUT:
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: LOCAL_LOCKOUT\n", __FUNCTION__);
            usbtmc_short_resp.bStatus = STATUS_SUCCESS;
            buf = &usbtmc_short_resp;
            resp_length = sizeof(usbtmc_short_resp.bStatus);

            /* Build and send a device clear message to the
             * application */
            controlMsg.type = USBTMC488_MSG_GOTO_LOCAL_LOCKOUT;
            controlMsg.value = 0;
            controlMsg.msg_buffer   = NULL;
            usbtmc_event_handler(&controlMsg, pData);

            break;

        default:
            usbtmc488_tsprintf("usbtmc488: %s: unhandled control\n"
                    "  setup->bRequest=%d\n",
                    __FUNCTION__, setup->bRequest);
            return(-1);
            break;
    }
    if ( resp_length )
    {
        status = TEMP_FAILURE_RETRY(write(fd, buf, resp_length));
        fsync(fd);

        localErrno = errno;
        if ( status != resp_length )
        {
            usbtmc488_tsprintf("usbtmc488: %s: Error writing control endpoint response\n"
                "localErrno=%d (%s)\n", __FUNCTION__, localErrno, strerror(localErrno));
            return(-1);
        }
    }
    else
    {
        usbtmcDbgPrint(USBTMC488_EP_CONTROL,
            "usbtmc488: %s: No control endpoint response queued!\n"
               "  setup->bRequest=%d\n",
               __FUNCTION__, setup->bRequest);
    }
    return(0);
}

PRIVATE void
handle_control(int fd, struct usb_ctrlrequest *setup)
{

//sjz printf("handle_control fd %d\n", fd);
    int             sem_status = 0,
                    sem_errno  = 0;
    int             localErrno = 0;
    int             status = 0;
    __u8            buf [256];
    __u16           value, index, length;

    value = __le16_to_cpu(setup->wValue);
    index = __le16_to_cpu(setup->wIndex);
    length = __le16_to_cpu(setup->wLength);

    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
        "%s: %s:%02x-%02x.%02x "
        "value=%04x index=%04x length=%d\n",
        __FUNCTION__,
        setup->bRequestType & USB_DIR_IN ? "IN" : "OUT",
        setup->bRequestType & USB_TYPE_MASK,
        setup->bRequestType & ~(USB_DIR_IN|USB_TYPE_MASK),
        setup->bRequest, value, index, length);

    if ( (setup->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD )
    {
        if ( handle_tmc_control(fd, setup) == 0 )
        {
            return;
        }
        else
        {
            usbtmc488_tsprintf("%s: handle_tmc_control returned error\n",
                __FUNCTION__);
            goto stall;
        }
    }

    /*
        These requests are to be handled by the kernel layer of the gadget
        driver and should not ever be seen here:
        USB_REQ_GET_STATUS
        USB_REQ_SET_FEATURE
        USB_REQ_GET_FEATURE
        USB_REQ_SET_ADDRESS

        USB_REQ_SET_DESCRIPTOR is optional and not implemented here.

        USB_REQ_SET_FEATURE is handled, in part, by the kernel layer of
        the gadget and then passed to this layer for further processing
        which is TDB.
    */
    switch ( setup->bRequest )      /* usb 2.0 spec ch9 requests */
    {
        case USB_REQ_GET_STATUS:
        {
            usbtmc488_tsprintf("%s: USB_REQ_GET_STATUS should be handled by the kernel "
                    "driver!!! requestType %02x\n",
                    __FUNCTION__, setup->bRequestType);
            return;
            break;
        }

        case USB_REQ_CLEAR_FEATURE:
        {
            index &= ~USB_DIR_IN;
            if ( (setup->bRequestType != USB_RECIP_ENDPOINT)
                 || ((index<=0) || (index>3))
                 || (length != 0)
                 || (USB_ENDPOINT_HALT != value) )
            {
                usbtmc488_tsprintf("%s: Unexpected USB_REQ_CLEAR_FEATURE packet:\n"
                    "  bRequestType=0x%02x, index=0x%02x, length=0x%02x, value=0x%02x\n"
                    "  setup->wValue=0x%02x, wIndex=0x%02x, wLength=0x%02x\n",
                    __FUNCTION__,
                    setup->bRequestType,
                    index,
                    length,
                    value,
                    setup->wValue,
                    setup->wIndex,
                    setup->wLength);
                goto stall;
            }

            errno = localErrno = status = 0;
            switch (index)
            {
                case BULKIN_ENDPOINT_ADDRESS:
                {
                    /* bulk_in */
                    WAITFOR(BULKIN_LOCK);
/* If the kernel layer handles the data toggle reset,
 * then don't ask it to do it again here.
 */
#ifdef NOKERNEL_CLEAR_HALT
                    if ( ioctl(bulkin_fd, FUNCTIONFS_CLEAR_HALT) < 0 )
                    {
                        localErrno = errno;
                        usbtmc488_tsprintf("%s: CLEAR_HALT error on bulkin: "
                            "localErrno=%d, error info: %s\n",
                            __FUNCTION__,
                            localErrno,
                            strerror(localErrno));
                        status = localErrno;
                    }
#endif /* NOKERNEL_CLEAR_HALT */
                    bulkin_state = BULK_EP_IDLE;
                    UNLOCK(BULKIN_LOCK);
                    break;
                }
                case BULKOUT_ENDPOINT_ADDRESS:
                {
                    /* bulk_out */
                    /* Can't wait for BULKOUT_LOCK here because it
                     * may already be locked by the usbtmc_request_thread
                     * or handle_tmc_control and we'd end up with a deadlock.
                     */
/* If the kernel layer handles the data toggle reset,
 * then don't ask it to do it again here.
 */
#ifdef NOKERNEL_CLEAR_HALT
                    if ( ioctl(bulkout_fd, FUNCTIONFS_CLEAR_HALT) < 0 )
                    {
                        localErrno = errno;
                        usbtmc488_tsprintf("%s: CLEAR_HALT error on bulkout: "
                            "localErrno=%d, error info: %s\n",
                            __FUNCTION__,
                            localErrno,
                            strerror(localErrno));
                        status = localErrno;
                    }
#endif /* NOKERNEL_CLEAR_HALT */
                    bulkout_state = BULK_EP_IDLE;
                    break;
                }
                case INTRPTIN_ENDPOINT_ADDRESS:
                {
/* If the kernel layer handles the data toggle reset,
 * then don't ask it to do it again here.
 */
#ifdef NOKERNEL_CLEAR_HALT
                    WAITFOR(INTRPT_LOCK);
                    usbtmcDbgPrint(USBTMC488_EP_INTRPT,
                        "%s: ioctl FUNCTIONFS_CLEAR_HALT\n", __FUNCTION__);
                    if ( ioctl(intr_fd, FUNCTIONFS_CLEAR_HALT) < 0 )
                    {
                        localErrno = errno;
                        usbtmc488_tsprintf("%s: CLEAR_HALT error on interrupt_in: "
                            "localErrno=%d, error info: %s\n",
                            __FUNCTION__,
                            localErrno,
                            strerror(localErrno));
                        status = localErrno;
                    }
                    usbtmcDbgPrint(USBTMC488_EP_INTRPT,
                        "%s: ioctl FUNCTIONFS_CLEAR_HALT DONE\n", __FUNCTION__);
                    UNLOCK(INTRPT_LOCK);
#endif /* NOKERNEL_CLEAR_HALT */
                    break;
                }
                default:
                {
                    usbtmc488_tsprintf("%s: Unexpected endpoint %d for CLEAR_HALT\n",
                        __FUNCTION__,
                        index);
                    status = -1;
                    break;
                }
            }
            if ( 0 != status )
            {
                usbtmc488_tsprintf("%s: status=%d, error info: %s\n",
                    __FUNCTION__, status, strerror(localErrno));
                goto stall;
            }
            /* ... ack (a write would stall) */
            status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
            if ( status )
            {
                usbtmc488_tsprintf("%s: ACK CLEAR_HALT ERROR: EP #%d: %s\n",
                    __FUNCTION__, index, strerror(localErrno));
            }
            return;
        }

        case USB_REQ_SET_FEATURE:
        {
            usbtmc488_tsprintf("%s: USB_REQ_SET_FEATURE should be handled by the kernel "
                    "driver!!!\n",
                    __FUNCTION__);
            return;
        }

        case USB_REQ_SET_ADDRESS:
        {
            usbtmc488_tsprintf("%s: USB_REQ_SET_ADDRESS should be handled by the kernel "
                    "driver!!!\n",
                    __FUNCTION__);
            return;
        }

        case USB_REQ_GET_DESCRIPTOR:
        {
            if ( setup->bRequestType != USB_DIR_IN )
            {
                usbtmc488_tsprintf("%s: USB_REQ_GET_DESCRIPTOR: bRequestType != USB_DIR_IN, bRequestType=0x%02x\n",
                        __FUNCTION__, setup->bRequestType);
                goto stall;
            }
            switch ( value >> 8 )
            {
                case USB_DT_STRING:
                {
                    int tmp;

                    WAITFOR(STRINGTAB_LOCK);
                    tmp = value & 0x0ff;
                    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                        "%s: ... get string %d lang %04x\n",
                        __FUNCTION__, tmp, index);
                    if ( tmp != 0 && index != strings->language )
                    {
                        UNLOCK(STRINGTAB_LOCK);
                        usbtmc488_tsprintf("%s: USB_REQ_GET_DESCRIPTOR: invalid string table request\n"
                                   "    value=0x%02x, index=0x%02x, strings.language=0x%02x\n"
                                   "    setup->wValue=0x%02x, wIndex=0x%02x, wLength=0x%02x\n",
                                   __FUNCTION__,
                                   value,
                                   index,
                                   strings->language,
                                   setup->wValue,
                                   setup->wIndex,
                                   setup->wLength);
                        goto stall;
                    }
#ifdef _CJS_DEBUG
                    // CJS
                    //status = usb_gadget_get_string(&strings, tmp, buf);
                    if ( status < 0 )
                    {
                        UNLOCK(STRINGTAB_LOCK);
                        usbtmc488_tsprintf("%s: USB_REQ_GET_DESCRIPTOR: usb_gadget_get_string error:\n"
                                   "    value=0x%02x, index=0x%02x, strings.language=0x%02x\n"
                                   "    setup->wValue=0x%02x, wIndex=0x%02x, wLength=0x%02x\n",
                                   __FUNCTION__,
                                   value,
                                   index,
                                   strings->language,
                                   setup->wValue,
                                   setup->wIndex,
                                   setup->wLength);
                        goto stall;
                    }
#endif // _CJS_DEBUG
                    UNLOCK(STRINGTAB_LOCK);
                    tmp = status;
                    if ( length < tmp )
                        tmp = length;
                    status = TEMP_FAILURE_RETRY(write(fd, buf, tmp));
                    fsync(fd);

                    if ( status < 0 )
                    {
                        localErrno = errno;
                        if ( localErrno == EIDRM )
                        {
                            usbtmc488_tsprintf(
                            "%s: USB_REQ_GET_DESCRIPTOR: USB_DT_STRING write timeout\n", __FUNCTION__);
                        }
                        else
                        {
                            usbtmc488_tsprintf("%s: write string data error: %s\n",
                                __FUNCTION__, strerror(localErrno));
                        }
                    }
                    else if ( status != tmp )
                    {
                        usbtmc488_tsprintf("%s: short string write, wrote %d of %d bytes\n",
                            __FUNCTION__,
                            status,
                            tmp);
                    }
                    break;
                }
                default:
                {
                    usbtmc488_tsprintf("%s: USB_REQ_GET_DESCRIPTOR: unexpected value requested\n"
                               "    value>>8=0x%02x\n",
                            __FUNCTION__, value>>8);
                    goto stall;
                    break;
                }
            }
            return;
        }

        case USB_REQ_SET_DESCRIPTOR:
        {
            usbtmc488_tsprintf("%s: USB_REQ_SET_DESCRIPTOR is not implemented",
                    __FUNCTION__);
            /* ... ack (a write would stall) */
            status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
            localErrno = errno;
            if ( status )
            {
                usbtmc488_tsprintf("%s: ACK USB_REQ_SET_DESCRIPTOR ERROR: %s\n",
                    __FUNCTION__, strerror(localErrno));
            }
            return;
        }

        case USB_REQ_GET_CONFIGURATION:
        {
            usbtmc488_tsprintf("%s: UNHANDLED ch9 REQUEST USB_REQ_GET_CONFIGURATION\n",
                __FUNCTION__);
            return;
        }

        case USB_REQ_SET_CONFIGURATION:
        {
            if ( setup->bRequestType != USB_DIR_OUT )
            {
                usbtmc488_tsprintf("%s: USB_REQ_SET_CONFIGURATION: invalid direction %d\n",
                    __FUNCTION__, setup->bRequestType);
                goto stall;
            }
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: CONFIG #%d\n", __FUNCTION__, value);

            /* Kernel is normally waiting for us to finish reconfiguring
             * the device.
             *
             * Some hardware can't, notably older PXA2xx hardware.  (With
             * racey and restrictive config change automagic.  PXA 255 is
             * OK, most PXA 250s aren't.  If it has a UDC CFR register,
             * it can handle deferred response for SET_CONFIG.)  To handle
             * such hardware, don't write code this way ... instead, keep
             * the endpoints always active and don't rely on seeing any
             * config change events, either this or SET_INTERFACE.
             */
            switch ( value )
            {
                case CONFIG_VALUE:
                    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                        "%s: Starting I/O\n", __FUNCTION__);
                    if ( 0 != start_io() )
                    {
                        usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                            "%s: I/O failed to start\n", __FUNCTION__);
                        return;
                    }
                    else
                    {
                        usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                            "%s: I/O started\n", __FUNCTION__);
                    }
                    break;
                case 0:
                    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                        "%s: Stopping I/O\n", __FUNCTION__);
                    stop_io();
                    break;
                default:
                    /* kernel bug -- "can't happen" */
                    usbtmc488_tsprintf("usbtmc488: %s: Unexpected SET_CONFIGURATION "
                               "value 0x%02x\n",
                                __FUNCTION__, value);
                    goto stall;
                    break;
            }

            /* ... ack (a write would stall) */
            status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
            localErrno = errno;
            if ( status )
            {
                usbtmc488_tsprintf("%s: ACK USB_REQ_SET_CONFIGURATION ERROR: %s\n",
                    __FUNCTION__, strerror(localErrno));
            }
            return;
        }

        case USB_REQ_GET_INTERFACE:
        {
            if ( setup->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE)
                 || index != 0
                 || length > 1 )
            {
                usbtmc488_tsprintf("%s: USB_REQ_GET_INTERFACE ERROR: %s\n"
                           "    setup->bRequestType=%d, index=%d, length=%d\n",
                    __FUNCTION__, setup->bRequestType, index, length);
                goto stall;
            }
            /* only one altsetting in this driver */
            buf [0] = 0;
            status = TEMP_FAILURE_RETRY(write(fd, buf, length));
            fsync(fd);

            localErrno = errno;
            if ( status < 0 )
            {
                if ( localErrno == EIDRM )
                {
                    usbtmc488_tsprintf(
                        "usbtmc488: %s: GET_INTERFACE timeout\n",
                        __FUNCTION__);
                }
                else
                {
                    perror("usbtmc488: write GET_INTERFACE data - error info");
                }
            }
            else if ( status != length )
            {
                usbtmc488_tsprintf("%s: short GET_INTERFACE write, %d\n", __FUNCTION__, status);
            }
            return;
        }

        case USB_REQ_SET_INTERFACE:
        {
            if ( setup->bRequestType != USB_RECIP_INTERFACE
                 || index != 0
                 || value != 0
               )
            {
                usbtmc488_tsprintf("%s: USB_REQ_SET_INTERFACE ERROR: %s\n"
                           "    setup->bRequestType=%d, index=%d, length=%d\n",
                    __FUNCTION__, setup->bRequestType, index, length);
                goto stall;
            }
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: INTERFACE #%d\n", __FUNCTION__, value);

            /* ... and ack (a write would stall) */
            status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
            localErrno = errno;
            if ( status )
            {
                usbtmc488_tsprintf("%s: ACK USB_REQ_SET_INTERFACE ERROR: %s\n",
                    __FUNCTION__, strerror(localErrno));
            }
            return;
        }

        case USB_REQ_SYNCH_FRAME:
        {
            usbtmc488_tsprintf("%s: UNHANDLED ch9 REQUEST USB_REQ_SYNCH_FRAME\n",
                __FUNCTION__);
            /* ... ack (a write would stall) */
            status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
            localErrno = errno;
            if ( status )
            {
                usbtmc488_tsprintf("%s: ACK USB_REQ_SYNCH_FRAME ERROR: %s\n",
                    __FUNCTION__, strerror(localErrno));
            }
            return;
        }

        default:
        {
            usbtmc488_tsprintf("%s: UNHANDLED ch9 REQUEST %02x\n",
                __FUNCTION__, setup->bRequest);
            return;
        }
    }
stall:
    usbtmc488_tsprintf("usbtmc488: %s: protocol stall\n"
        "  setup->bRequestType  = 0x%02x\n"
        "  setup->bRequest      = 0x%02x\n",
        __FUNCTION__, setup->bRequestType, setup->bRequest);

    /* non-iso endpoints are stalled by issuing an i/o request
     * in the "wrong" direction.  ep0 is special only because
     * the direction isn't fixed.
     */
    if ( setup->bRequestType & USB_DIR_IN )
    {
        status = TEMP_FAILURE_RETRY(read(fd, buf, 0));
    }
    else
    {
        status = TEMP_FAILURE_RETRY(write(fd, buf, 0));
        fsync(fd);
    }
    if ( status != -1 )
    {
        usbtmc488_tsprintf( "usbtmc488: %s: can't stall ep0 for\n"
            "  setup->bRequestType  = 0x%02x\n"
            "  setup->bRequest      = 0x%02x\n",
        __FUNCTION__, setup->bRequestType, setup->bRequest);
    }
    else if ( errno != EL2HLT )
    {
        //perror("usbtmc488: ep0 stall - error info");
        usbtmc488_tsprintf( "usbtmc488: %s: ep0 stall errno %d\n"
            "  setup->bRequestType  = 0x%02x\n"
            "  setup->bRequest      = 0x%02x\n",
        __FUNCTION__, errno, setup->bRequestType, setup->bRequest);
    }
}

/*-------------------------------------------------------------------------*/

/* control thread, handles main event loop  */

#define MAXEVENTS       5

PRIVATE void
ep0_thread_cleanup( void *fd_ptr )
{

    (void)fd_ptr;
    close_fd(&ep0_fd);
    ep0_fd = -ENODEV;
    usbtmcDbgPrint(USBTMC488_THREADS, "%s: done\n", __FUNCTION__);
}

PRIVATE void *
ep0_thread(void *param)
{
    int fd         = *((int *) param);
    int localErrno = 0;

printf("ep0_thread fd is %d\n", fd);
    //ep0_fd = fd;

    ep0_tid = pthread_self();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    usbtmcThreadArray[USBTMC488_EP0_THREAD].pid = syscall(__NR_gettid);
#else /* LINUX_VERSION_CODE */
    usbtmcThreadArray[USBTMC488_EP0_THREAD].pid = getpid();
#endif /* LINUX_VERSION_CODE */
    strncpy(usbtmcThreadArray[USBTMC488_EP0_THREAD].name,
        "usbtmc EPO thread",
            USBTMC_MAX_THREADNAME_LENGTH);

    pthread_cleanup_push(ep0_thread_cleanup, &ep0_fd);

    MaxTransferSize = 0;
    MaxPacketSize = 0;

    /* event loop */
    for ( ;; )
    {
        int                         tmp;
        struct usb_functionfs_event event [MAXEVENTS];
        int                         i, nevent;
        BOOL                        connected = FALSE;

        pthread_testcancel();
        errno = localErrno = 0;

        tmp = TEMP_FAILURE_RETRY(read(fd, &event, sizeof(event)));
        localErrno = errno;
        if ( tmp < 0 )
        {
            if ( localErrno == EAGAIN )
            {
                usbtmc488_sleep(1);
                continue;
            }
            usbtmc488_tsprintf("%s: ep0 read error: fd=%d, localErrno=%d (%s)\n",
                __FUNCTION__,
                fd,
                localErrno,
                strerror(localErrno));
            goto done;
        }
        nevent = tmp / sizeof(event[0]);
        if ( nevent != 1 )
        {
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: read %d ep0 events\n",
                __FUNCTION__, nevent);
        }

        for ( i = 0; i < nevent; i++ )
        {
            switch ( event [i].type )
            {
                case FUNCTIONFS_ENABLE:
                    //usbtmc488_tsprintf("usbtmc488: %s: EP0 EVENT: ENABLE\n", __FUNCTION__);
                    MaxTransferSize = MAX_TRANSFER_SIZE_HS;
                    MaxPacketSize = MAX_PACKET_SIZE_HS;
                    current_speed = USB_SPEED_HIGH;
                    connected = 1;
                    start_io();
                    break;
                case FUNCTIONFS_SETUP:
                    connected = 1;
                    handle_control(fd, &event [i].u.setup);
                    break;
                case FUNCTIONFS_UNBIND:
                case FUNCTIONFS_DISABLE:
                    connected = 0;
                    current_speed = USB_SPEED_UNKNOWN;
#if 0
                    if (event [i].type == FUNCTIONFS_UNBIND)
                        usbtmc488_tsprintf("usbtmc488: %s: EP0 EVENT: UNBIND\n", __FUNCTION__);
                    else
                        usbtmc488_tsprintf("usbtmc488: %s: EP0 EVENT: DISABLE\n", __FUNCTION__);
#endif
                    /* Allow the request thread to run so the kernel layer can return
                     * -1 with errno=ESHUTDOWN for the bulkout read which will be
                     * pending.
                     */
                    usbtmc488_sleep(100);
                    usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                            "%s: (%s:%d) Stopping I/O\n",
                            __FUNCTION__, __FILE__, __LINE__);
                    stop_io();
                    break;
                case FUNCTIONFS_SUSPEND:
                    /* connected = 1; */
                    //usbtmc488_tsprintf("usbtmc488: %s: EP0 EVENT: SUSPEND\n", __FUNCTION__);
                    break;
                case FUNCTIONFS_BIND:
                    //usbtmc488_tsprintf("usbtmc488: %s: EP0 EVENT: BIND\n", __FUNCTION__);
                    break;
                default:
                    usbtmc488_tsprintf( "usbtmc488: %s: " "EP0 EVENT: unhandled event %d\n",
                            __FUNCTION__, event [i].type);
                    break;
            }
        }
        continue;
        done:
        if ( connected )
        {
            usbtmcDbgPrint(USBTMC488_EP_CONTROL,
                "%s: (%s:%d) Stopping I/O\n",
                __FUNCTION__, __FILE__, __LINE__);
            stop_io();
        }
        break;
    }

    pthread_cleanup_pop(1);
    return(0);
}

/*-------------------------------------------------------------------------*/

PRIVATE int
initialize_driver( void )
{
    static BOOL bFatalError = FALSE;
    int         localErrno  = 0;
    int ii;

    if ( bFatalError )
    {
        return -1;
    }

    /* Initialize the thread array */
    memset(usbtmcThreadArray, 0, sizeof(usbtmcThreadArray));

    for (ii=0; ii<NUM_LOCKS; ii++)
    {
        /* Initialize as not shareable between processes and available */
        if ( 0 > sem_init(pLockArray[ii],0,1) )
        {
            localErrno = errno;
            usbtmc488_tsprintf(
                "%s: Fatal initialization error\n"
                "  sem_init(%s) error: %s\n",
                __FUNCTION__, lockTypeStrings[ii], strerror(localErrno));
            bFatalError = TRUE;
            return -1;
        }
    }
    /* Initialize these semaphores separately because they cannot be used
     * by the macros WAITFOR, UNLOCK, etc.
     */
    /* Initialize as not shareable between processes and available */
    if ( 0 > sem_init(&debugLock,0,1) )
    {
        localErrno = errno;
        usbtmc488_tsprintf(
            "%s: Fatal initialization error\n"
            "  sem_init(&debugLock) error: %s\n",
            __FUNCTION__, strerror(localErrno));
        bFatalError = TRUE;
        return -1;
    }
    /* Initialize as not shareable between processes and available */
    if ( 0 > sem_init(&tsPrintLock,0,1) )
    {
        localErrno = errno;
        usbtmc488_tsprintf(
            "%s: Fatal initialization error\n"
            "  sem_init(&tsPrintLock) error: %s\n",
            __FUNCTION__, strerror(localErrno));
        bFatalError = TRUE;
        return -1;
    }
    driver_initialized = TRUE;
    return 0;
}

USBTMC488_STATUS
usbtmc488_get_thread_info(unsigned int                *returnedNumThreads,
              const USBTMC488_THREAD_INFO **pThreadArray)
{
    *returnedNumThreads = USBTMC488_NUM_THREADS;
    *pThreadArray = &usbtmcThreadArray[0];
    return USBTMC488_SUCCESS;
}

/*-------------------------------------------------------------------------*/
USBTMC488_STATUS
usbtmc488_enable_interface( const  USBTMC488_DEVICE_INFO *device_info,
                void   (*event_handler)(const USBTMC488_MESSAGE *msg, void* pData), void* pData)
{
    int sem_status = 0, sem_errno = 0;
    int localErrno = 0;
    int status;
    int len;
    pthread_attr_t attr;
    char *p;

    /* Perform the startup initialization if this is the first time
     * we've been called.
     */
    if ( FALSE == driver_initialized )
    {
        if ( 0 != initialize_driver() )
        {
            usbtmc488_tsprintf("%s: Failed to initialize driver!\n",
                __FUNCTION__);
            return USBTMC488_INTERFACE_NOT_ENABLED;
        }
    }

    WAITFOR(USER_SERVICE_LOCK);

    USBTMC488_STATUS   retValue = USBTMC488_SUCCESS;
    static int fd = -EINVAL;
    const unsigned int maxResponseBufSizeFS =
        MAX_TRANSFER_SIZE_FS+sizeof(struct usbtmc_bulk_in_msg);
    const unsigned int maxRequestBufSizeFS =
        MAX_TRANSFER_SIZE_FS+sizeof(struct usbtmc_bulk_out_msg);
    const unsigned int maxResponseBufSizeHS =
        MAX_TRANSFER_SIZE_HS+sizeof(struct usbtmc_bulk_in_msg);
    const unsigned int maxRequestBufSizeHS =
        MAX_TRANSFER_SIZE_HS+sizeof(struct usbtmc_bulk_out_msg);
    const unsigned int bufferAllocSize = ALIGNMENT +
        max(max(maxResponseBufSizeFS, maxRequestBufSizeFS),
            max(maxResponseBufSizeHS, maxRequestBufSizeHS));

    if ( NULL == device_info )
    {
        usbtmc488_tsprintf("%s: NULL device info\n", __FUNCTION__);
        retValue = USBTMC488_INTERFACE_NOT_ENABLED;
        goto uei_exit;
    }

    if ( FALSE != usbtmc_interface_enabled )
    {
        usbtmcDbgPrint(USBTMC488_API,
            "%s: already enabled\n", __FUNCTION__);
        retValue = USBTMC488_INTERFACE_ALREADY_ENABLED;
        goto uei_exit;
    }

    memset(&current_device_info, 0, sizeof(current_device_info));
    memcpy(&current_device_info, device_info, sizeof(current_device_info));

    /* Clear the static response message header and the last bulkout header */
    memset(&last_req_dev_dep_msgin_msg, 0, sizeof(last_req_dev_dep_msgin_msg));
    memset(&last_bulkout_hdr, 0, sizeof(last_bulkout_hdr));

    if ( NULL == event_handler )
    {
        usbtmcDbgPrint(USBTMC488_API,
            "%s: event handler is NULL\n", __FUNCTION__);
        retValue = USBTMC488_INTERFACE_NOT_ENABLED;
        goto uei_exit;
    }
    else
    {
        usbtmc_event_handler = event_handler;
    }

    device_desc.idVendor = __cpu_to_le16(current_device_info.vendorId);
    device_desc.idProduct = __cpu_to_le16(current_device_info.productId);
    device_desc.bcdDevice = __cpu_to_le16(current_device_info.productVersion);

    WAITFOR(STRINGTAB_LOCK);

    len = sizeof(struct usb_functionfs_stringtab) +
          strlen(current_device_info.manufacturerDescr) + 1 +
          strlen(current_device_info.productDescr) + 1 +
          strlen(current_device_info.productSerialNumberDescr) + 1 +
          strlen(current_device_info.configurationDescr) + 1 +
          strlen(current_device_info.interfaceDescr) + 1;

    strings = (struct usb_functionfs_stringtab *) malloc(len);
    strings->header.magic = __cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC);
    strings->header.length = len;
    strings->header.str_count = __cpu_to_le32(NUM_STRINGIDS);
    strings->header.lang_count = __cpu_to_le32(1);
    strings->language = __cpu_to_le16(LANGUAGE_ID_ENGLISH_US);
    p = strings->str;

    // STRINGID_MFGR = 1
    strcpy(p, current_device_info.manufacturerDescr);
    p += strlen(current_device_info.manufacturerDescr) + 1;

    // STRINGID_PRODUCT = 2
    strcpy(p, current_device_info.productDescr);
    p += strlen(current_device_info.productDescr) + 1;

    // STRINGID_SERIAL = 3
    strcpy(p, current_device_info.productSerialNumberDescr);
    p += strlen(current_device_info.productSerialNumberDescr) + 1;

    // STRINGID_CONFIG = 4
    strcpy(p, current_device_info.configurationDescr);
    p += strlen(current_device_info.configurationDescr) + 1;

    // STRINGID_INTERFACE = 5
    strcpy(p, current_device_info.interfaceDescr);
    p += strlen(current_device_info.interfaceDescr) + 1;

    UNLOCK(STRINGTAB_LOCK);

    if ( USBTMC488_SUCCESS == retValue )
    {
        bulkout_thread = usbtmc_request_thread;
        intrpt_thread = usbtmc_intrpt_thread;

        fd = init_device();
//sjz printf("usbtmc488_enable_interface fd is %d\n", fd);

        if ( fd < 0 )
        {
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }
        usbtmc488_sleep(100);
        status = pthread_attr_init(&attr);
        if ( 0 != status )
        {
            usbtmc488_tsprintf("%s: pthread_attr_init failed: status=%d (%s)\n",
                    __FUNCTION__, status, strerror(status));
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }
        status = pthread_attr_setstacksize(&attr, USBTMC488_STACK_SIZE);
        if ( 0 != status )
        {
            usbtmc488_tsprintf("%s: pthread_attr_setstacksize failed: status=%d (%s)\n",
                    __FUNCTION__, status, strerror(status));
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }
        if ( pthread_create(&ep0_tid, &attr, ep0_thread, (void *) &fd) != 0 )
        {
            localErrno = errno;
            usbtmc488_tsprintf("%s: error creating ep0 thread: localErrno=%d (%s)\n",
                    __FUNCTION__, localErrno, strerror(localErrno));
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }

        pthread_setname_np(ep0_tid, "USBTMCcntl0");

        status = pthread_attr_destroy(&attr);
        if ( 0 != status )
        {
            usbtmc488_tsprintf("%s: pthread_attr_destroy failed: status=%d (%s)\n",
                    __FUNCTION__, status, strerror(status));
        }
        if ( NULL != pResponseMsg )
        {
            usbtmc488_tsprintf("%s: pResponseMsg not NULL!\n", __FUNCTION__);
        }
        if ( NULL != pRequestBuffer )
        {
            usbtmc488_tsprintf("%s: pRequestBuffer not NULL!\n", __FUNCTION__);
        }
        if ( 0 != posix_memalign((void **)&pResponseMsg,
                    sizeof(void *),
                    (size_t) bufferAllocSize) )
        {
            usbtmc488_tsprintf(
                    "usbtmc488: %s: Unable to alloc %d bytes for "
                    "pResponseMsg errno %d\n",
                    __FUNCTION__, bufferAllocSize, errno);
            pResponseMsg = NULL;
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }
        if ( 0 != posix_memalign((void **)&pRequestBuffer,
                    sizeof(void *),
                    (size_t) bufferAllocSize) )
        {
            usbtmc488_tsprintf(
                    "usbtmc488: %s: Unable to alloc %d bytes for "
                    "pRequestBuffer\n",
                    __FUNCTION__, bufferAllocSize);
            free(pResponseMsg);
            pResponseMsg = NULL;
            pRequestBuffer = NULL;
            retValue = USBTMC488_INTERFACE_NOT_ENABLED;
            goto uei_exit;
        }
        usbtmc_interface_enabled = TRUE;
    }

uei_exit:
    responseState = IDLE;
    bulkin_inprogress = BULKIN_IDLE;
    bulkin_state = bulkout_state = BULK_EP_IDLE;
    UNSIGNAL_EVENT(TALKADDR_OR_TERMINATE);
    UNSIGNAL_EVENT(INTRPT_EVENT);
    UNSIGNAL_EVENT(INTRPT_DONE_EVENT);
    if ( USBTMC488_SUCCESS == retValue )
    {
        //usbtmc488_tsprintf("%s: USBTMC enabled; response buffer size = %d\n",
        //    __FUNCTION__, bufferAllocSize);
    }
    else
    {
        usbtmc488_tsprintf("%s: USBTMC not enabled\n", __FUNCTION__);
    }
    UNLOCK(USER_SERVICE_LOCK);

    usbtmc488_sleep(500);

    return(retValue);
}

/*-------------------------------------------------------------------------*/
USBTMC488_STATUS
usbtmc488_disable_interface( void )
{
    int sem_status = 0, sem_errno = 0;
    int status     = 0;

    if ( (FALSE == driver_initialized) || (FALSE == usbtmc_interface_enabled) )
    {
        return(USBTMC488_INTERFACE_NOT_ENABLED);
    }
    WAITFOR(USER_SERVICE_LOCK);
    UNLOCK(REQUEST_LOCK);
    UNLOCK(DEVICE_CLEAR_LOCK);
    UNLOCK(BULKIN_LOCK);
    UNLOCK(BULKOUT_LOCK);
    UNLOCK(INTRPT_LOCK);
    UNSIGNAL_EVENT(TALKADDR_OR_TERMINATE);
    UNSIGNAL_EVENT(INTRPT_EVENT);
    UNSIGNAL_EVENT(INTRPT_DONE_EVENT);
    usbtmcDbgPrint(USBTMC488_API,
        "%s: (%s:%d) Stopping I/O\n",
        __FUNCTION__, __FILE__, __LINE__);
    stop_io();
    if ( NULL != pResponseMsg )
    {
        free(pResponseMsg);
        pResponseMsg = NULL;
    }
    if ( NULL != pRequestBuffer )
    {
        free(pRequestBuffer);
        pRequestBuffer = NULL;
    }
    if ( -1 != (int)ep0_tid )
    {
        status = pthread_cancel(ep0_tid);
        if ( 0 != status )
        {
            usbtmc488_tsprintf("%s: error %d (%s) cancelling ep0 thread\n",
                __FUNCTION__, status, strerror(status));
        }
        status = pthread_join(ep0_tid, 0);
        if ( status != 0 )
        {
            usbtmc488_tsprintf("%s: error returned from pthread_join(ep0_tid): %d (%s)\n",
                __FUNCTION__, status, strerror(status));
        }
        ep0_tid = (pthread_t) -1;
    }
    usbtmc_interface_enabled = FALSE;
    usbtmc_event_handler = NULL;
    memset(&current_device_info, 0, sizeof(current_device_info));
    /* Allow time for the request thread to exit before returning control
     * to the application in case it immediately re-enables the interface.
     */
    usbtmc488_sleep(100);
    //usbtmc488_tsprintf("%s: USBTMC disabled\n", __FUNCTION__);

    UNLOCK(USER_SERVICE_LOCK);
    return(USBTMC488_SUCCESS);
}

BOOL usbtmc488_interface_enabled( void )
{
    int sem_status = 0, sem_errno = 0;
    if ( FALSE == driver_initialized )
    {
        return(FALSE);
    }
    WAITFOR(USER_SERVICE_LOCK);
    int state = usbtmc_interface_enabled;
    UNLOCK(USER_SERVICE_LOCK);
    return(state);
}

USBTMC488_STATUS
usbtmc488_message( const USBTMC488_MESSAGE *msg )
{
    int         sem_status = 0, sem_errno = 0;
    int         timed_wait_status    = 0;
    static BOOL response_in_progress = FALSE;

    /* We can't take the USER_SERVICE_LOCK here because it could result in a
     * deadlock when the gadget is responding to a data request that the
     * application has fulfilled but the host is taking it a small portion at
     * a time. During this process, the application will call this function
     * to send another message, such as a USBTMC488_MSG_DATA_WITH_EOM or
     * USBTMC488_MSG_FREE_REQUEST_MSG message, to the gadget when
     * we're still doing work in queue_response_msg().
     */

    USBTMC488_STATUS retStatus = USBTMC488_SUCCESS;

    if ( (FALSE == driver_initialized) || (FALSE == usbtmc_interface_enabled) )
    {
        retStatus = USBTMC488_INTERFACE_NOT_ENABLED;
        goto um_exit;
    }
    switch ( msg->type )
    {
    case USBTMC488_MSG_DATA_WITHOUT_EOM:
    case USBTMC488_MSG_DATA_WITH_EOM:

        //usbtmc488_tsprintf("%s MSG_DATA_WITH/WITHOUT_EOM\n", __FUNCTION__);
        //usbtmc488_tsprintf("SENDING MESSAGE: %s\n", msg->msg_buffer->buffer);

        if ( TRUE == response_in_progress )
        {
            retStatus = USBTMC488_NO_DATA_REQUESTED;
            goto um_exit;
        }
        else
        {
            response_in_progress = TRUE;
        }

        retStatus = queue_response_msg(msg);
        response_in_progress = FALSE;
        break;
    case USBTMC488_MSG_UPDATE_STATUS_BYTE:
        //printf("%s MSG_UPDATE_STATUS_BYTE\n", __func__);
        WAITFOR(STATUS_BYTE_LOCK);
        usbtmc_status_byte = msg->value;
        UNLOCK(STATUS_BYTE_LOCK);
        break;
    case USBTMC488_MSG_REQUEST_SERVICE:
        //printf("%s MSG_REQUEST_SERVICE\n", __func__);
        WAITFOR(STATUS_BYTE_LOCK);
        WAITFOR(INTRPT_LOCK);
        usbtmc_status_byte = msg->value;
        /* Setup and send the interrupt-in packet */
        usbtmc_interrupt_in_resp.bNotify1 = 0x81;
        usbtmc_interrupt_in_resp.bNotify2 = usbtmc_status_byte;
        /* Allow any bulkin transfers to complete because the kernel driver (dwc_otg)
         * shares a FIFO for bulkin and interrupt-in - sheesh!
         */
        WAITFOR(BULKIN_LOCK);
        /* Signal the interrupt thread to write the interrupt-in response */
        usbtmcDbgPrint(USBTMC488_EP_CONTROL,
            "%s: signal 2 INTRPT_EVENT\n", __FUNCTION__);
        SIGNAL_EVENT(INTRPT_EVENT);
        /* Wait for the interrupt thread to write the response or a timeout. If
         * the host does not send IN tokens on interrupt-in, then the INTRPT_DONE_EVENT
         * will remain locked and the interrupt thread will be stuck in it's
         * write to the interrupt-in file descriptor.
         */
        timed_wait_status = timed_wait(INTRPT_DONE_EVENT, INTRPT_TIMEOUT);
        if ( (0 != timed_wait_status) && (ETIMEDOUT == errno) )
        {
            usbtmc488_tsprintf("%s: MSG_REQUEST_SERVICE Timeout (%d msec) waiting for "
                "INTRPT_DONE_EVENT\n",
                __FUNCTION__, INTRPT_TIMEOUT);
        }
        else if ( 0 != timed_wait_status )
        {
            usbtmc488_tsprintf("%s: Error %d (%s) waiting for INTRPT_DONE_EVENT\n",
                __FUNCTION__, timed_wait_status, strerror(timed_wait_status));
        }
        UNLOCK(BULKIN_LOCK);
        UNLOCK(INTRPT_LOCK);
        UNLOCK(STATUS_BYTE_LOCK);
        break;
    case USBTMC488_MSG_CANCEL_IO:
        //printf("%s MSG_CANCEL_IO\n", __func__);
        WAITFOR(RESPONSE_LOCK);
        responseState = TERMINATE;
        bulkin_inprogress = BULKIN_IDLE;
        bulkin_state = BULK_EP_IDLE;
        SIGNAL_EVENT(TALKADDR_OR_TERMINATE);
        response_in_progress = FALSE;
        UNLOCK(RESPONSE_LOCK);
        break;
    case USBTMC488_MSG_DEVICE_CLEAR_DONE:
//sjz        printf("%s MSG_DEVICE_CLEAR_DONE\n", __func__);
        UNLOCK(DEVICE_CLEAR_LOCK);
        break;
    case USBTMC488_MSG_FREE_REQUEST_MSG:
//sjz        printf("%s MSG_FREE_REQUEST_MSG\n", __func__);
        UNLOCK(REQUEST_LOCK);
        break;
    default:
        usbtmc488_tsprintf("usbtmc488: %s: Bad msg type (%d)\n",
            __FUNCTION__, msg->type);
        retStatus = USBTMC488_FAILED;
    }
um_exit:
    return(retStatus);
}

PRIVATE const char *
statusStrings[USBTMC488_NUM_STATUSES] =
{
    "USBTMC488_SUCCESS",
    "USBTMC488_FAILED",
    "USBTMC488_INTERFACE_ALREADY_ENABLED",
    "USBTMC488_INTERFACE_NOT_ENABLED",
    "USBTMC488_NO_DATA_REQUESTED"
};

PRIVATE const char *
msgTypeStrings[USBTMC488_NUM_MSG_TYPES] =
{
    "USBTMC488_MSG_DEVICE_CLEAR",
    "USBTMC488_MSG_DATA_WITHOUT_EOM",
    "USBTMC488_MSG_DATA_WITH_EOM",
    "USBTMC488_MSG_DATA_REQUESTED",
    "USBTMC488_MSG_DEVICE_TRIGGER",
    "USBTMC488_MSG_UPDATE_STATUS_BYTE",
    "USBTMC488_MSG_REQUEST_SERVICE",
    "USBTMC488_MSG_REN_ENABLE",
    "USBTMC488_MSG_REN_DISABLE",
    "USBTMC488_MSG_GOTO_LOCAL",    
    "USBTMC488_MSG_CANCEL_IO",
    "USBTMC488_MSG_DEVICE_CLEAR_DONE",
    "USBTMC488_MSG_FREE_REQUEST_MSG",
    "USBTMC488_MSG_GOTO_LOCAL_LOCKOUT",
    "USBTMC488_MSG_UPDATE_LOCAL_STATUS_BYTE"
};

const char *
usbtmc488_status_to_string(USBTMC488_STATUS status)
{
    if ( (0 > status) || (USBTMC488_NUM_STATUSES <= status) )
    {
        return("error: unknown status");
    }
    else
    {
        return(statusStrings[status]);
    }
}

const char *
usbtmc488_msg_type_to_string(USBTMC488_MSG_TYPE type)
{
    if ( (0 > type) || (USBTMC488_NUM_MSG_TYPES <= type) )
    {
        return("error: unknown msg type");
    }
    else
    {
        return(msgTypeStrings[type]);
    }
}

void
usbtmc488_set_verbosity( unsigned int v )
{
    verbosity = v;
}

unsigned int
usbtmc488_get_verbosity( void )
{
    return(verbosity);
}

const char *
usbtmc488_show_verbosity_bits( void )
{
    return(verbosity_table);
}

void usbtmc488_tsprintf( const char *pFmt, ... )
{
    int sem_status = 0, sem_errno = 0;

    if ( TRUE == driver_initialized )
    {
        if ( 0 != (sem_status = sem_wait(&tsPrintLock)) )
        {
            sem_errno = errno;
            usbtmc488_tsprintf(
                "usbtmc488: %s: error locking tsPrintLock semaphore\n"
                "  error: %d (%s)\n",
                __FUNCTION__,
                sem_status,
                strerror(sem_errno));
        }
    }
    va_list   argPtr;
    struct timeval   timeVal;
    struct timezone  tz;
    unsigned int     msecs, usecs;
    char             clockString[64];
    char             timeOfDay[16];

    if ( (0 > gettimeofday(&timeVal, &tz)) ||
         (NULL == ctime_r(&timeVal.tv_sec, clockString)) ||
         (1 != sscanf(clockString, "%*s %*s %*s %s", timeOfDay))
       )
    {
        printf("%s: Unable to get time of day\n", __FUNCTION__);
    }
    else
    {
        msecs = (unsigned int)(timeVal.tv_usec/1000);
        usecs = (unsigned int)(timeVal.tv_usec%1000);
        printf("%s.%03u.%03u ", timeOfDay, msecs, usecs);
    }

    va_start(argPtr, pFmt);
    vprintf(pFmt, argPtr);
    fflush(stdout);
    va_end(argPtr);
    if ( TRUE == driver_initialized )
    {
        if ( 0 != (sem_status = sem_post(&tsPrintLock)) )
        {
            sem_errno = errno;
            usbtmc488_tsprintf(
                "usbtmc488: %s: error unlocking tsPrintLock semaphore\n"
                "  error: %d (%s)\n",
                __FUNCTION__,
                sem_status,
                strerror(sem_errno));
        }
    }
    fflush(stdout);
}

PRIVATE void
usbtmc488_sleep( unsigned int numMilliseconds )
{
    struct timespec     aLittleNap;
    struct timespec     timeLeft;

    aLittleNap.tv_sec = (time_t)(numMilliseconds / 1000);
    aLittleNap.tv_nsec = (time_t)((numMilliseconds % 1000) * 1000000);
    if ( (0 == aLittleNap.tv_sec) && (0 == aLittleNap.tv_nsec) )
    {
        aLittleNap.tv_nsec = 1000;
    }

    nanosleep(&aLittleNap, &timeLeft);
}

PRIVATE int
timed_wait( USBTMC488_LOCK_TYPE lockType, int msecs )
{
    int             sem_status = 0;
    int             localErrno = 0;
    struct timespec timeout, now;
    long            secs, nsecs;

    usbtmcDbgPrint(USBTMC488_SYNCH,
        "%s: %s: %d ENTRY\n",
        __FUNCTION__, lockTypeStrings[lockType], msecs);

    if ( 0 != clock_gettime(CLOCK_REALTIME, &now) )
    {
        localErrno = errno;
        usbtmc488_tsprintf("%s: Error returned from clock_gettime: %s\n",
            __FUNCTION__, strerror(localErrno));
        return(EINVAL);
    }

    secs = 0;
    nsecs = now.tv_nsec + (msecs * 1000000);
    if ( nsecs >= 1000000000 )
    {
        secs = nsecs / 1000000000;
        nsecs = nsecs % 1000000000;
    }
    timeout.tv_sec = now.tv_sec + secs;
    timeout.tv_nsec = nsecs;

    sem_status = sem_timedwait(pLockArray[lockType], &timeout);

    usbtmcDbgPrint(USBTMC488_SYNCH,
        "%s: %s: %d EXIT\n",
        __FUNCTION__, lockTypeStrings[lockType], msecs);

    return(sem_status);

}

