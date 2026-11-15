#include "app.h"
#include "os.h"
#include "bsp_potentiometer.h"
#include "bsp_buzzer.h"
#include "bsp_keypad.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "bsp_ntc.h"
#include "bsp_motor.h"
#include "bsp_nixie.h"
#include "bsp_sdcard.h"
#include "bsp_usb_msc.h"
#include "bsp_spi_flash.h"
#include "bsp_efs.h"
#include "bsp_st7789.h"
#include "bsp_cst816.h"
#include "bsp_lvgl.h"
#include "app_cli.h"
#include "systick.h"
#include <stdio.h>

/* 0：屏蔽 USB MSC（MCU->U盘模式 TF）；改 1 可再开 */
#ifndef APP_USB_MSC_ENABLE
#define APP_USB_MSC_ENABLE  0
#endif

static uint8_t s_usb_msc_on = 0U;

static uint16_t s_pot_raw = 0;
static uint8_t  s_key = 0;
static uint8_t  s_btn = 0;
static int16_t  s_ntc_temp_x10 = 0;

static uint8_t  s_sd_ok = 0;
static uint8_t  s_sd_rw_ok = 0;
static uint8_t  s_sd_diag_hold = 0;
static uint32_t s_sd_size_mb = 0;

static void sd_show_absent(void)
{
    s_sd_ok = 0U;
    s_sd_rw_ok = 0U;
    s_sd_size_mb = 0U;
    s_sd_diag_hold = 1U;
    bsp_led_all_off();
    bsp_nixie_show_left(0U, 0U);
    bsp_nixie_show_right(0U, 0U);
}

static void sd_show_fail_diag(void)
{
    s_sd_diag_hold = 1U;
    /* 左=阶段 右=错误码（同官方 sd_error_enum） */
    bsp_led_write((uint8_t)((1U << g_sd_fail_stage) - 1U));
    bsp_nixie_show_left((uint16_t)g_sd_fail_stage, 0U);
    bsp_nixie_show_right((uint16_t)g_sd_fail_code, 0U);
    printf("[SD] FAIL stage=%u code=%lu\r\n",
           (unsigned)g_sd_fail_stage, (unsigned long)g_sd_fail_code);
}

/*!
    \brief  对齐官方 Read_write：init → 就绪后刷新数码管容量
*/
static void tf_card_selftest(void)
{
    uint32_t sectors;

    s_sd_ok = 0U;
    s_sd_rw_ok = 0U;
    s_sd_size_mb = 0U;
    s_sd_diag_hold = 0U;

    if(0U == bsp_sd_is_present()) {
        printf("[SD] no card\r\n");
        sd_show_absent();
        return;
    }

    printf("[SD] init...\r\n");
    if(BSP_SD_OK != bsp_sd_init()) {
        sd_show_fail_diag();
        return;
    }

    sectors = bsp_sd_get_sector_count();
    s_sd_size_mb = sectors / 2048U;
    s_sd_ok = 1U;
    s_sd_rw_ok = 1U;
    printf("[SD] OK capacity=%lu MB (%lu GB)\r\n",
           (unsigned long)s_sd_size_mb,
           (unsigned long)(s_sd_size_mb / 1024U));

    s_sd_diag_hold = 1U;
    bsp_nixie_show_left((uint16_t)(s_sd_size_mb / 1024U), 0U);

#if APP_USB_MSC_ENABLE
    /* 挂 USB MSC：电脑认 TF 为 U 盘 */
    if(0U == bsp_usb_msc_init()) {
        s_usb_msc_on = 1U;
        bsp_led_write(0x0FU);
        bsp_nixie_show_right(8U, 0U); /* 右=8：USB MSC 已开 */
        printf("[USB] plug cable to PC to browse TF card\r\n");
    } else {
        bsp_nixie_show_right(9U, 0U);
        printf("[USB] MSC init fail\r\n");
    }
#else
    s_usb_msc_on = 0U;
    bsp_led_write(0x03U); /* 低 2 灯：仅 SD 就绪，无 USB 盘 */
    bsp_nixie_show_right(0U, 0U);
    printf("[USB] MSC disabled (MCU-only TF access)\r\n");
#endif
}

static void tf_card_on_remove(void)
{
    printf("[SD] removed\r\n");
    bsp_sd_deinit();
    s_usb_msc_on = 0U; /* MSC 无卡可读；下次插入再挂 */
    sd_show_absent();
    bsp_buzzer_beep(40U);
}

static void task_sd_detect(void)
{
    bsp_sd_evt_t evt = bsp_sd_detect_task();

    if(BSP_SD_EVT_INSERT == evt) {
        printf("[SD] inserted\r\n");
        bsp_buzzer_beep(30U);
        if(0U != s_usb_msc_on) {
            printf("[USB] MSC active, defer remount\r\n");
            return;
        }
        tf_card_selftest();
    } else if(BSP_SD_EVT_REMOVE == evt) {
        tf_card_on_remove();
    }
}

static void task_potentiometer(void)
{
    s_pot_raw = bsp_potentiometer_get_raw();
    if(0U != s_sd_diag_hold) {
        return;
    }
    bsp_nixie_show_right(s_pot_raw, 1U);
}

static void task_buzzer(void)
{
    bsp_buzzer_task();
    bsp_motor_task();
}

static void task_keypad(void)
{
    uint8_t key;
    keypad_event_t evt;

    bsp_keypad_task();

    evt = bsp_keypad_get_event(&key);
    if(KEYPAD_EVT_PRESS == evt) {
        s_key = key;
        bsp_buzzer_beep(30U);
        bsp_motor_vibrate(50U);
        if(key <= LED_NUM) {
            bsp_led_write((uint8_t)(1U << (key - 1U)));
        } else if(key <= 16U) {
            bsp_led_write((uint8_t)(1U << (key - 9U)));
        }
    } else if(KEYPAD_EVT_RELEASE == evt) {
        if(key == s_key) {
            s_key = KEYPAD_KEY_NONE;
        }
    }
}

static void task_button(void)
{
    uint8_t id;
    btn_event_t evt;

    bsp_button_task();

    evt = bsp_button_get_event(&id);
    if(BTN_EVT_PRESS == evt) {
        s_btn = id;
        bsp_buzzer_beep(30U);
        bsp_motor_vibrate(80U);
        if((id >= BTN_S17) && (id <= BTN_S20)) {
            bsp_led_write((uint8_t)(1U << (id - BTN_S17)));
        }
        if(BTN_S20 == id) {
            if(0U != s_usb_msc_on) {
                /* 电脑挂载中禁止再写自检，避免和 MSC 抢卡 */
                printf("[USB] MSC active, skip SD selftest\r\n");
            } else {
                tf_card_selftest();
            }
        }
    } else if(BTN_EVT_RELEASE == evt) {
        if(id == s_btn) {
            s_btn = BTN_NONE;
        }
    }
}

static void task_ntc(void)
{
    bsp_ntc_task();
    s_ntc_temp_x10 = bsp_ntc_get_temp_x10();
    if(0U != s_sd_diag_hold) {
        return;
    }
    bsp_nixie_show_temp_x10(s_ntc_temp_x10);
}

void app_init(void)
{
    os_init();

    os_task_add(task_potentiometer, 10U);
    os_task_add(task_buzzer, 1U);
    os_task_add(task_keypad, 10U);
    os_task_add(task_button, 10U);
    os_task_add(task_ntc, 100U);
    os_task_add(task_sd_detect, 50U); /* TF DET 防抖 + 插拔事件 */
    os_task_add(app_cli_task, 5U);    /* 串口指令树（写文件需更勤轮询） */
    /* 触摸改由 LVGL indev 处理 */

    s_pot_raw = 0U;
    s_key = KEYPAD_KEY_NONE;
    s_btn = BTN_NONE;
    s_ntc_temp_x10 = 0;

    bsp_buzzer_set_volume(80U);
    bsp_motor_set_duty(70U);
    /* 初始化阶段先关掉，避免 USB 初始化耗时/中断导致 task 跑不到而一直响 */
    bsp_buzzer_off();
    bsp_motor_off();

    bsp_led_all_off();
    bsp_nixie_show_left(0U, 1U);
    bsp_nixie_show_right(0U, 1U);

    app_cli_init();

    /* 外置 Flash 文件表（映射表在 SPI Flash 首扇区） */
    if(EFS_OK != bsp_efs_init()) {
        printf("[EFS] init fail\r\n");
    }

    bsp_sd_detect_init();
    tf_card_selftest();

    /* ZD25Q32D：SPI0+DMA，读 JEDEC ID + 末页擦写校验 */
    {
        uint8_t mid = 0U, mtype = 0U, cap = 0U;
        uint8_t wbuf[16];
        uint8_t rbuf[16];
        uint32_t i;
        uint32_t test_addr = SPI_FLASH_SIZE - SPI_FLASH_SECTOR_SIZE;

        printf("[FLASH] SPI0 PA4~7 init...\r\n");
        if(SPI_FLASH_OK != bsp_spi_flash_init()) {
            printf("[FLASH] init fail\r\n");
        } else if(SPI_FLASH_OK != bsp_spi_flash_read_id(&mid, &mtype, &cap)) {
            printf("[FLASH] RDID fail\r\n");
        } else {
            printf("[FLASH] JEDEC ID=%02X %02X %02X\r\n", mid, mtype, cap);
            for(i = 0U; i < 16U; i++) {
                wbuf[i] = (uint8_t)(0xA0U + i);
                rbuf[i] = 0U;
            }
            if(SPI_FLASH_OK != bsp_spi_flash_sector_erase(test_addr)) {
                printf("[FLASH] sector erase fail\r\n");
            } else if(SPI_FLASH_OK != bsp_spi_flash_write(test_addr, wbuf, 16U)) {
                printf("[FLASH] write fail\r\n");
            } else if(SPI_FLASH_OK != bsp_spi_flash_read(test_addr, rbuf, 16U)) {
                printf("[FLASH] read fail\r\n");
            } else {
                for(i = 0U; i < 16U; i++) {
                    if(rbuf[i] != wbuf[i]) {
                        printf("[FLASH] verify fail @%lu\r\n", (unsigned long)i);
                        break;
                    }
                }
                if(16U == i) {
                    printf("[FLASH] R/W OK (SPI0+DMA)\r\n");
                }
            }
        }
    }

    /* ST7789 + CST816T：扩展板原理图引脚 */
    {
        uint8_t chip = 0U;
        uint8_t fw = 0U;

        printf("[LCD] ST7789 SPI0+DMA init...\r\n");
        bsp_st7789_init();
        printf("[LCD] ready\r\n");

        printf("[TP] CST816 I2C PB6/7 init...\r\n");
        if(CST816_OK != bsp_cst816_init()) {
            printf("[TP] init fail\r\n");
        } else if(CST816_OK != bsp_cst816_read_id(&chip, &fw)) {
            printf("[TP] read id fail\r\n");
        } else {
            printf("[TP] ChipID=%02X Fw=%u\r\n", chip, (unsigned)fw);
        }

        if(0U == bsp_lvgl_init()) {
            bsp_lvgl_task_start();
        }
    }

    /* 首帧 LVGL 可能较久：先关蜂鸣器，避免任务饿死时一直响 */
    bsp_buzzer_off();
    bsp_motor_off();

    printf("[OS] app_init done, starting scheduler (port=%d)...\r\n", (int)OS_PORT);
}

void app_run(void)
{
    os_run();
}
