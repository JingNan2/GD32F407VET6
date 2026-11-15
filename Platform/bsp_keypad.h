#ifndef BSP_KEYPAD_H
#define BSP_KEYPAD_H

#include <stdint.h>

#define KEYPAD_KEY_NONE     0U
#define KEYPAD_KEY_MAX      16U

typedef enum {
    KEYPAD_EVT_NONE = 0,
    KEYPAD_EVT_PRESS,
    KEYPAD_EVT_RELEASE
} keypad_event_t;

void bsp_keypad_init(void);

/* 10ms 周期任务中调用：扫描 + 消抖 */
void bsp_keypad_task(void);

/* 当前稳定按下的键号 1~16，无键为 0 */
uint8_t bsp_keypad_get_key(void);

/* 取走一次事件（按下/松开），无事件返回 KEYPAD_EVT_NONE */
keypad_event_t bsp_keypad_get_event(uint8_t *key);

#endif /* BSP_KEYPAD_H */
