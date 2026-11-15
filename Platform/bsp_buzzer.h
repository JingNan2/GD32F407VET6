#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdint.h>

void bsp_buzzer_init(void);

/* 开/关（按当前音量输出 4kHz PWM） */
void bsp_buzzer_on(void);
void bsp_buzzer_off(void);

/* 音量 0~100；内部映射为占空比 0%~50%（无源蜂鸣器 50% 最响） */
void bsp_buzzer_set_volume(uint8_t volume);
uint8_t bsp_buzzer_get_volume(void);

/* 非阻塞鸣叫 ms 毫秒 */
void bsp_buzzer_beep(uint32_t ms);

/* 1ms 任务中调用 */
void bsp_buzzer_task(void);

#endif /* BSP_BUZZER_H */
