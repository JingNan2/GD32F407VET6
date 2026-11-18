#ifndef PLATFORM_H
#define PLATFORM_H

#include "gd32f4xx.h"
#include "bsp_clock.h"
#include "bsp_rtc.h"
#include "bsp_potentiometer.h"
#include "bsp_buzzer.h"
#include "bsp_keypad.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "bsp_ntc.h"
#include "bsp_motor.h"
#include "bsp_nixie.h"
#include "bsp_sdcard.h"
#include "bsp_usart.h"
#include "bsp_usb_msc.h"
#include "bsp_spi0_bus.h"
#include "bsp_spi_flash.h"
#include "bsp_efs.h"
#include "bsp_fatfs.h"
#include "bsp_lv_fs_efs.h"
#include "bsp_st7789.h"
#include "bsp_cst816.h"
#include "bsp_lvgl.h"

/* 平台统一初始化：时钟 / SysTick / 板级外设 */
void platform_init(void);

#endif /* PLATFORM_H */
