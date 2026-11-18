/*!
    \file    bsp_lvgl.c
    \brief   LVGL 显示/触摸驱动 + 文件管理主界面
*/

#include "bsp_lvgl.h"
#include "bsp_st7789.h"
#include "bsp_cst816.h"
#include "app_ui_files.h"
#include "systick.h"
#include "os.h"
#include "lvgl.h"
#include <stdio.h>

#define LVGL_BUF_LINES   40U
#define LVGL_HOR_RES     ST7789_WIDTH
#define LVGL_VER_RES     ST7789_HEIGHT

static lv_display_t *s_disp;
static lv_indev_t   *s_indev;
static uint8_t s_ready;
static uint8_t s_flush_dbg;
static uint16_t s_last_tx;
static uint16_t s_last_ty;

static uint8_t s_buf1[LVGL_HOR_RES * LVGL_BUF_LINES * 2U] __attribute__((aligned(4)));
static uint8_t s_buf2[LVGL_HOR_RES * LVGL_BUF_LINES * 2U] __attribute__((aligned(4)));

static uint8_t s_lvgl_first;
static volatile uint8_t s_lvgl_paused;

static uint32_t lvgl_tick_get(void)
{
    return sys_tick_get();
}

static void lvgl_delay(uint32_t ms)
{
    delay_1ms(ms);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t w;
    uint16_t h;

    w = (uint16_t)(area->x2 - area->x1 + 1);
    h = (uint16_t)(area->y2 - area->y1 + 1);
    bsp_st7789_blit((uint16_t)area->x1, (uint16_t)area->y1, w, h, (const uint16_t *)px_map);
    lv_display_flush_ready(disp);

    if(s_flush_dbg < 2U) {
        s_flush_dbg++;
        printf("[LVGL] flush #%u %ux%u\r\n", (unsigned)s_flush_dbg, (unsigned)w, (unsigned)h);
    }
}

static void lvgl_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x = 0U;
    uint16_t y = 0U;

    (void)indev;
    bsp_cst816_scan();
    if(0U != bsp_cst816_is_pressed()) {
        bsp_cst816_get_xy(&x, &y);
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (int32_t)x;
        data->point.y = (int32_t)y;
        s_last_tx = x;
        s_last_ty = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_last_tx = 0xFFFFU;
        s_last_ty = 0xFFFFU;
    }
}

void bsp_lvgl_pause(void)
{
    if(0U == s_lvgl_paused) {
        s_lvgl_paused = 1U;
        printf("[LVGL] pause (DMA -> UART RX)\r\n");
    }
}

void bsp_lvgl_resume(void)
{
    if(0U != s_lvgl_paused) {
        s_lvgl_paused = 0U;
        if((0U != s_ready) && (NULL != s_disp)) {
            lv_obj_invalidate(lv_screen_active());
            app_ui_files_refresh();
        }
        printf("[LVGL] resume (DMA -> SPI/LCD)\r\n");
    }
}

static void lvgl_thread(void)
{
    if(0U != s_lvgl_first) {
        s_lvgl_first = 0U;
        printf("[LVGL] task running, first refr...\r\n");
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(s_disp);
        printf("[LVGL] first refr done\r\n");
    }

#if (OS_PORT == OS_PORT_FREERTOS)
    for(;;) {
        if(0U == s_lvgl_paused) {
            app_ui_files_poll();
            (void)lv_timer_handler();
        }
        os_delay_ms(5U);
    }
#else
    if(0U == s_lvgl_paused) {
        app_ui_files_poll();
        (void)lv_timer_handler();
    }
#endif
}

uint8_t bsp_lvgl_init(void)
{
    lv_draw_buf_t *buf;
    uint32_t buf_sz;

    s_ready = 0U;
    s_flush_dbg = 0U;
    s_last_tx = 0xFFFFU;
    s_last_ty = 0xFFFFU;
    s_lvgl_paused = 0U;

    lv_init();
    lv_tick_set_cb(lvgl_tick_get);
    lv_delay_set_cb(lvgl_delay);

    s_disp = lv_display_create((int32_t)LVGL_HOR_RES, (int32_t)LVGL_VER_RES);
    if(NULL == s_disp) {
        printf("[LVGL] display create fail\r\n");
        return 1U;
    }
    lv_display_set_default(s_disp);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    buf = lv_display_get_buf_active(s_disp);
    buf_sz = lv_display_get_draw_buf_size(s_disp);
    if((NULL == buf) || (NULL == buf->data) || (0U == buf_sz)) {
        printf("[LVGL] draw buffer NOT set\r\n");
        return 1U;
    }

    s_indev = lv_indev_create();
    if(NULL == s_indev) {
        printf("[LVGL] indev create fail\r\n");
        return 1U;
    }
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, lvgl_touch_read);
    lv_indev_set_display(s_indev, s_disp);

    /* 主界面：文件管理 */
    app_ui_files_create(lv_screen_active());

    s_ready = 1U;
    printf("[LVGL] file UI OK %ux%u\r\n",
           (unsigned)LVGL_HOR_RES, (unsigned)LVGL_VER_RES);
    return 0U;
}

void bsp_lvgl_task_start(void)
{
    if(0U == s_ready) {
        return;
    }
    s_lvgl_first = 1U;
    /* exFAT/LFN 目录扫描栈开销较大 */
    if(0 != os_thread_add(lvgl_thread, "lvgl", 4096U, 5U, 5U)) {
        printf("[LVGL] os_thread_add fail\r\n");
    }
}
