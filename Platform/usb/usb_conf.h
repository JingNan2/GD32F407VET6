/*!
    \file    usb_conf.h
    \brief   USBFS device core config（官方 msc_udisk，去掉 eval 板依赖）
*/
#ifndef USB_CONF_H
#define USB_CONF_H

#include "gd32f4xx.h"

#ifdef USE_USB_FS
#define USB_FS_CORE
#endif

#ifdef USE_USB_HS
#define USB_HS_CORE
#endif

#ifdef USB_FS_CORE
#define RX_FIFO_FS_SIZE                         128U
#define TX0_FIFO_FS_SIZE                        64U
#define TX1_FIFO_FS_SIZE                        128U
#define TX2_FIFO_FS_SIZE                        0U
#define TX3_FIFO_FS_SIZE                        0U
#define USBFS_SOF_OUTPUT                        0U
#define USBFS_LOW_POWER                         0U
#endif

#define USE_DEVICE_MODE

#ifndef USB_FS_CORE
#ifndef USB_HS_CORE
#error "USB_HS_CORE or USB_FS_CORE should be defined!"
#endif
#endif

#ifndef USE_DEVICE_MODE
#ifndef USE_HOST_MODE
#error "USE_DEVICE_MODE or USE_HOST_MODE should be defined!"
#endif
#endif

#ifndef USE_USB_HS
#ifndef USE_USB_FS
#error "USE_USB_HS or USE_USB_FS should be defined!"
#endif
#endif

#ifdef USB_HS_INTERNAL_DMA_ENABLED
#if defined(__GNUC__) && !defined(__clang__)
#define __ALIGN_END __attribute__((aligned(4U)))
#define __ALIGN_BEGIN
#else
#define __ALIGN_END
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#define __ALIGN_BEGIN __attribute__((aligned(4U)))
#elif defined(__ICCARM__)
#define __ALIGN_BEGIN
#else
#define __ALIGN_BEGIN
#endif
#endif
#else
#define __ALIGN_BEGIN
#define __ALIGN_END
#endif

/* AC5/AC6/GCC：库里用 __packed 做非对齐访问；AC6 无内置该关键字，置空即可 */
#ifdef __packed
#undef __packed
#endif
#define __packed

#endif /* USB_CONF_H */
