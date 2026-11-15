#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdint.h>

/* 与原理图丝印一致：S17~S20 */
#define BTN_NONE        0U
#define BTN_S17         17U
#define BTN_S18         18U
#define BTN_S19         19U
#define BTN_S20         20U
#define BTN_NUM         4U

typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_PRESS,
    BTN_EVT_RELEASE
} btn_event_t;

void bsp_button_init(void);

/* 10ms 周期任务中调用：扫描 + 消抖 */
void bsp_button_task(void);

/* 查询某键当前是否按下（id = BTN_S17~BTN_S20） */
uint8_t bsp_button_is_pressed(uint8_t id);

/* 取走一次事件；无事件返回 BTN_EVT_NONE */
btn_event_t bsp_button_get_event(uint8_t *id);

#endif /* BSP_BUTTON_H */
