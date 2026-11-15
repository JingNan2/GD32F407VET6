#ifndef BSP_USB_MSC_H
#define BSP_USB_MSC_H

#include <stdint.h>
#include "drv_usb_dev.h"

extern usb_core_driver g_msc_udisk;

/* 须先 bsp_sd_init 成功再调用 */
uint8_t bsp_usb_msc_init(void);
uint8_t bsp_usb_msc_sd_ready(void);
void    usb_timer_irq(void);
void    usbd_storage_set_block_count(uint32_t sectors);

#endif /* BSP_USB_MSC_H */
