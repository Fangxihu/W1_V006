/* Copyright (c) 2017 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */

/* Definitions for USB hub trapset. */

#ifndef APP_USB_HUB_IF_H
#define APP_USB_HUB_IF_H

/** USB device configuration parameter ids. */
typedef enum usb_device_cfg_key
{
    USB_DEVICE_CFG_VENDOR_ID = 0,
    USB_DEVICE_CFG_PRODUCT_ID = 1,
    USB_DEVICE_CFG_VERSION = 2,
    USB_DEVICE_CFG_DEVICE_CLASS_CODES = 3,
    USB_DEVICE_CFG_LANGID = 4,
    USB_DEVICE_CFG_MAX_POWER = 5,
    USB_DEVICE_CFG_REMOTE_WAKE_DISABLE = 6,
    USB_DEVICE_CFG_BCD_DEVICE = 7
} usb_device_cfg_key;

/**
 * USB device configuration parameters.
 *
 * Any parameter with corresponding bit in the "ValidParams" field set to "0"
 * is ignored. */
typedef struct usb_device_parameters
{
    /** idVendor field in Standard Device Descriptor. */
    uint16 VendorID;
    /** idProduct field in Standard Device Descriptor. */
    uint16 ProductID;
    /** bcdUSB field in Standard Device Descriptor. */
    uint16 Version;
    /** bDeviceClass, bDeviceSubClass and bDeviceProtocol fields in
     * Standard Device Descriptor. */
    uint8 DeviceClassCodes[3];
    /** Supported LANGID for string descriptors */
    uint16 LangID;
    /** bMaxPower in Standard Configuration Descriptor */
    uint16 MaxPower;
    /** Affects bmAttributes.Remote Wakeup in Standard Configuration Descriptor */
    uint16 RemoteWakeDisable;
    /** bcdDevice field in Standard Device Descriptor. */
    uint16 BCDDevice;

    /** Any parameter with corresponding bit set to "0" is ignored.
     * "usb_device_cfg_key" gives bit position for every parameter. */
    uint32 ValidParams;
} usb_device_parameters;

#endif /* APP_USB_HUB_IF_H */
