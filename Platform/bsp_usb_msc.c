/*!
    \file    bsp_usb_msc.c
    \brief   USBFS MSC：电脑认作 U 盘，后端 TF(SDIO)
*/

#include "bsp_usb_msc.h"
#include "bsp_sdcard.h"
#include "drv_usb_hw.h"
#include "usbd_msc_core.h"
#include <stdio.h>

usb_core_driver g_msc_udisk;
static uint8_t s_sd_for_msc = 0U;

uint8_t bsp_usb_msc_sd_ready(void)
{
    return s_sd_for_msc;
}

uint8_t bsp_usb_msc_init(void)
{
    {
        uint32_t sectors = bsp_sd_get_sector_count();
        if(0U == sectors) {
            s_sd_for_msc = 0U;
            return 1U;
        }
        /* 必须在 usbd_init/枚举前写入容量，否则 Windows 会当成未格式化盘 */
        usbd_storage_set_block_count(sectors);
    }
    s_sd_for_msc = 1U;

    usb_gpio_config();
    usb_rcu_config();
    usb_timer_init();

    usbd_init(&g_msc_udisk,
              USB_CORE_ENUM_FS,
              &msc_desc,
              &msc_class);

    usb_intr_config();

    printf("[USB] MSC ready: TF as U-disk (PA11/PA12 USBFS)\r\n");
    printf("[USB] sectors=%lu\r\n", (unsigned long)bsp_sd_get_sector_count());
    return 0U;
}
