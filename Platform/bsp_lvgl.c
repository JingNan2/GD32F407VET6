/*!
    \file    bsp_lvgl.c
    \brief   LVGL：ST7789 + CST816 + 片内 RTC 指针表盘
*/

#include "bsp_lvgl.h"
#include "bsp_st7789.h"
#include "bsp_cst816.h"
#include "bsp_rtc.h"
#include "systick.h"
#include "os.h"
#include "lvgl.h"
#include <stdio.h>

#define LVGL_BUF_LINES   40U
#define LVGL_HOR_RES     ST7789_WIDTH
#define LVGL_VER_RES     ST7789_HEIGHT

#define WATCH_CX         120
#define WATCH_CY         140
#define WATCH_R          100
#define HAND_HOUR_R      55
#define HAND_MIN_R       75
#define HAND_SEC_R       88
#define TICK_OUTER_R     96
#define TICK_INNER_R     86
#define TICK_HOUR_INNER  78

static lv_display_t *s_disp;
static lv_indev_t   *s_indev;
static uint8_t s_ready;
static uint8_t s_flush_dbg;
static uint16_t s_last_tx;
static uint16_t s_last_ty;

static lv_obj_t *s_hand_hour;
static lv_obj_t *s_hand_min;
static lv_obj_t *s_hand_sec;
static lv_point_precise_t s_pts_hour[2];
static lv_point_precise_t s_pts_min[2];
static lv_point_precise_t s_pts_sec[2];
static lv_obj_t *s_lbl_digital;

/* sin(0..90°) * 256 */
static const int16_t s_sin256[91] = {
    0, 4, 9, 13, 18, 22, 27, 31, 36, 40, 44, 49, 53, 58, 62, 66, 71, 75, 79, 83,
    88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 139, 143, 147,
    150, 154, 158, 161, 165, 168, 171, 175, 178, 181, 184, 187, 190, 193, 196, 199,
    202, 204, 207, 210, 212, 215, 217, 219, 222, 224, 226, 228, 230, 232, 234, 236,
    237, 239, 241, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 254,
    255, 255, 255, 256, 256, 256, 256
};

/* 双 partial 缓冲：DMA 须在主 SRAM */
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

static int16_t isin256(uint16_t deg)
{
    deg = (uint16_t)(deg % 360U);
    if(deg <= 90U) {
        return s_sin256[deg];
    }
    if(deg <= 180U) {
        return s_sin256[180U - deg];
    }
    if(deg <= 270U) {
        return (int16_t)(-s_sin256[deg - 180U]);
    }
    return (int16_t)(-s_sin256[360U - deg]);
}

static int16_t icos256(uint16_t deg)
{
    return isin256((uint16_t)(deg + 90U));
}

static void polar_to_xy(int16_t r, uint16_t deg, int32_t *x, int32_t *y)
{
    *x = WATCH_CX + ((int32_t)r * (int32_t)isin256(deg)) / 256;
    *y = WATCH_CY - ((int32_t)r * (int32_t)icos256(deg)) / 256;
}

static void hand_set(lv_point_precise_t pts[2], lv_obj_t *line, int16_t r, uint16_t deg)
{
    int32_t x;
    int32_t y;

    polar_to_xy(r, deg, &x, &y);
    pts[0].x = (lv_value_precise_t)WATCH_CX;
    pts[0].y = (lv_value_precise_t)WATCH_CY;
    pts[1].x = (lv_value_precise_t)x;
    pts[1].y = (lv_value_precise_t)y;
    lv_line_set_points_mutable(line, pts, 2);
}

static void watch_update_hands(void)
{
    bsp_rtc_time_t t;
    uint16_t deg_h;
    uint16_t deg_m;
    uint16_t deg_s;
    char buf[20];

    if(BSP_RTC_OK != bsp_rtc_get(&t)) {
        t.hour = 0U;
        t.minute = 0U;
        t.second = 0U;
    }

    deg_s = (uint16_t)(t.second * 6U);
    deg_m = (uint16_t)(t.minute * 6U + t.second / 10U);
    deg_h = (uint16_t)(((t.hour % 12U) * 30U) + (t.minute / 2U));

    hand_set(s_pts_hour, s_hand_hour, HAND_HOUR_R, deg_h);
    hand_set(s_pts_min, s_hand_min, HAND_MIN_R, deg_m);
    hand_set(s_pts_sec, s_hand_sec, HAND_SEC_R, deg_s);

    if(NULL != s_lbl_digital) {
        (void)lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                          (unsigned)t.hour, (unsigned)t.minute, (unsigned)t.second);
        lv_label_set_text(s_lbl_digital, buf);
    }
}

static void watch_timer_cb(lv_timer_t *tmr)
{
    (void)tmr;
    watch_update_hands();
}

static void add_tick(lv_obj_t *parent, uint16_t deg, int16_t r0, int16_t r1, int32_t w, uint32_t color)
{
    lv_obj_t *line;
    static lv_point_precise_t pts_pool[12][2];
    static uint8_t idx = 0U;
    lv_point_precise_t *pts;
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;

    if(idx >= 12U) {
        return;
    }
    pts = pts_pool[idx++];
    polar_to_xy(r0, deg, &x0, &y0);
    polar_to_xy(r1, deg, &x1, &y1);
    pts[0].x = (lv_value_precise_t)x0;
    pts[0].y = (lv_value_precise_t)y0;
    pts[1].x = (lv_value_precise_t)x1;
    pts[1].y = (lv_value_precise_t)y1;

    line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_width(line, w, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t w;
    uint16_t h;
    uint32_t t0 = 0U;
    uint32_t t1;

    w = (uint16_t)(area->x2 - area->x1 + 1);
    h = (uint16_t)(area->y2 - area->y1 + 1);

    if(s_flush_dbg < 2U) {
        t0 = sys_tick_get();
    }

    bsp_st7789_blit((uint16_t)area->x1, (uint16_t)area->y1, w, h, (const uint16_t *)px_map);
    lv_display_flush_ready(disp);

    if(s_flush_dbg < 2U) {
        t1 = sys_tick_get();
        s_flush_dbg++;
        printf("[LVGL] flush #%u %d,%d %ux%u %lums\r\n",
               (unsigned)s_flush_dbg,
               (int)area->x1, (int)area->y1,
               (unsigned)w, (unsigned)h,
               (unsigned long)(t1 - t0));
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
        if((x != s_last_tx) || (y != s_last_ty)) {
            s_last_tx = x;
            s_last_ty = y;
            printf("[TP] x=%u y=%u\r\n", (unsigned)x, (unsigned)y);
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_last_tx = 0xFFFFU;
        s_last_ty = 0xFFFFU;
    }
}

static void lvgl_ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *dial;
    lv_obj_t *dot;
    uint8_t i;

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B1020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    /* 表盘圆 */
    dial = lv_obj_create(scr);
    lv_obj_remove_style_all(dial);
    lv_obj_set_size(dial, (WATCH_R * 2) + 4, (WATCH_R * 2) + 4);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dial, lv_color_hex(0x1A233A), 0);
    lv_obj_set_style_bg_opa(dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dial, 3, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(0x4CC9F0), 0);
    lv_obj_set_pos(dial, WATCH_CX - WATCH_R - 2, WATCH_CY - WATCH_R - 2);
    lv_obj_set_scrollable(dial, false);

    /* 12 时刻度 */
    for(i = 0U; i < 12U; i++) {
        uint16_t deg = (uint16_t)(i * 30U);
        int16_t r_in = (0U == (i % 3U)) ? TICK_HOUR_INNER : TICK_INNER_R;
        int32_t tw = (0U == (i % 3U)) ? 3 : 2;
        uint32_t col = (0U == (i % 3U)) ? 0xEAEAEAU : 0x6C757DU;
        add_tick(scr, deg, TICK_OUTER_R, r_in, tw, col);
    }

    /* 时针 */
    s_hand_hour = lv_line_create(scr);
    lv_obj_remove_style_all(s_hand_hour);
    lv_obj_set_style_line_width(s_hand_hour, 5, 0);
    lv_obj_set_style_line_color(s_hand_hour, lv_color_hex(0xEAEAEA), 0);
    lv_obj_set_style_line_rounded(s_hand_hour, true, 0);

    /* 分针 */
    s_hand_min = lv_line_create(scr);
    lv_obj_remove_style_all(s_hand_min);
    lv_obj_set_style_line_width(s_hand_min, 3, 0);
    lv_obj_set_style_line_color(s_hand_min, lv_color_hex(0xADB5BD), 0);
    lv_obj_set_style_line_rounded(s_hand_min, true, 0);

    /* 秒针 */
    s_hand_sec = lv_line_create(scr);
    lv_obj_remove_style_all(s_hand_sec);
    lv_obj_set_style_line_width(s_hand_sec, 2, 0);
    lv_obj_set_style_line_color(s_hand_sec, lv_color_hex(0xF72585), 0);
    lv_obj_set_style_line_rounded(s_hand_sec, true, 0);

    /* 中心轴点 */
    dot = lv_obj_create(scr);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xF72585), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_pos(dot, WATCH_CX - 5, WATCH_CY - 5);

    /* 底部数字时间 */
    s_lbl_digital = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_digital, lv_color_hex(0x4CC9F0), 0);
    lv_obj_align(s_lbl_digital, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_label_set_text(s_lbl_digital, "--:--:--");

    watch_update_hands();
    (void)lv_timer_create(watch_timer_cb, 200, NULL);
    lv_obj_invalidate(scr);
}

void bsp_lvgl_pause(void)
{
    s_lvgl_paused = 1U;
}

void bsp_lvgl_resume(void)
{
    s_lvgl_paused = 0U;
}

static void lvgl_thread(void)
{
    if(0U != s_lvgl_first) {
        s_lvgl_first = 0U;
        printf("[LVGL] task running, first refr...\r\n");
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(s_disp);
        printf("[LVGL] first refr done, flushes=%u\r\n", (unsigned)s_flush_dbg);
    }

#if (OS_PORT == OS_PORT_FREERTOS)
    for(;;) {
        if(0U == s_lvgl_paused) {
            (void)lv_timer_handler();
        }
        os_delay_ms(5U);
    }
#else
    if(0U == s_lvgl_paused) {
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

    printf("[LVGL] tick=%lu buf@%p size=%u\r\n",
           (unsigned long)sys_tick_get(),
           (void *)s_buf1,
           (unsigned)sizeof(s_buf1));

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
        printf("[LVGL] draw buffer NOT set. buf=%p sz=%lu\r\n",
               (void *)buf, (unsigned long)buf_sz);
        return 1U;
    }
    printf("[LVGL] draw buf OK data=%p sz=%lu cf=%d\r\n",
           (void *)buf->data, (unsigned long)buf_sz,
           (int)lv_display_get_color_format(s_disp));

    s_indev = lv_indev_create();
    if(NULL == s_indev) {
        printf("[LVGL] indev create fail\r\n");
        return 1U;
    }
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, lvgl_touch_read);
    lv_indev_set_display(s_indev, s_disp);

    lvgl_ui_create();

    s_ready = 1U;
    printf("[LVGL] watchface OK %ux%u\r\n",
           (unsigned)LVGL_HOR_RES, (unsigned)LVGL_VER_RES);
    return 0U;
}

void bsp_lvgl_task_start(void)
{
    if(0U == s_ready) {
        return;
    }
    s_lvgl_first = 1U;
    if(0 != os_thread_add(lvgl_thread, "lvgl", 2048U, 5U, 5U)) {
        printf("[LVGL] os_thread_add fail\r\n");
    }
}
