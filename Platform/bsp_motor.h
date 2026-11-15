#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdint.h>

void bsp_motor_init(void);

/* 占空比 0~100，直接控制震动强度 */
void bsp_motor_set_duty(uint8_t duty);
uint8_t bsp_motor_get_duty(void);

void bsp_motor_on(void);   /* 按当前占空比启动 */
void bsp_motor_off(void);  /* 占空比输出 0 */

/* 非阻塞震动 ms 毫秒 */
void bsp_motor_vibrate(uint32_t ms);

/* 1ms 任务中调用 */
void bsp_motor_task(void);

#endif /* BSP_MOTOR_H */
