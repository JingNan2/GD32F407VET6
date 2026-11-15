/*!
    \file    usbd_conf.h
    \brief   USB device configuration（官方 msc_udisk）
*/
#ifndef USBD_CONF_H
#define USBD_CONF_H

#include "usb_conf.h"

#define USBD_CFG_MAX_NUM                1U
#define USBD_ITF_MAX_NUM                1U
#define USB_STR_DESC_MAX_SIZE           64U

#define USBD_MSC_INTERFACE              0U

#define MSC_IN_EP                       EP1_IN
#define MSC_OUT_EP                      EP1_OUT

#ifdef USE_USB_HS
#ifdef USE_ULPI_PHY
#define MSC_DATA_PACKET_SIZE            512U
#else
#define MSC_DATA_PACKET_SIZE            64U
#endif
#else
#define MSC_DATA_PACKET_SIZE            64U
#endif

#define MSC_MEDIA_PACKET_SIZE           4096U
#define MEM_LUN_NUM                     1U
#define USB_STRING_COUNT                4U

#endif /* USBD_CONF_H */
