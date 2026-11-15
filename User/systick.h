#ifndef SYS_TICK_H
#define SYS_TICK_H

#include <stdint.h>

/* 配置 SysTick 为 1ms 中断 */
void systick_config(void);

/* 硬件 SysTick 累计 ms（OS 启动前后都递增；os_tick_ms 可能另有映射） */
uint32_t systick_ms_get(void);

/* 系统运行时间（ms，走 OS 抽象） */
uint32_t sys_tick_get(void);

/* 判断从 start 起是否已过 ms 毫秒（处理溢出） */
uint8_t sys_tick_expired(uint32_t start, uint32_t ms);

/* 仅用于初始化阶段的阻塞延时；任务中禁止使用 */
void delay_1ms(uint32_t count);

/* SysTick_Handler 中调用 */
void systick_isr(void);

#endif /* SYS_TICK_H */
