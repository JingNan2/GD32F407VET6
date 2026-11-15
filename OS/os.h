#ifndef OS_H
#define OS_H

#include <stdint.h>
#include "os_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*os_task_fn)(void);

/*! 初始化任务表（不启动调度） */
void os_init(void);

/*!
    注册周期任务（应用层统一接口）
    \param period_ms  执行周期；0 视为 1ms
    \return 0 成功，-1 失败
*/
int os_task_add(os_task_fn fn, uint32_t period_ms);

/*!
    注册“线程式”任务：后端自行解释
    - FreeRTOS：独立任务，fn 内可死循环 + os_delay_ms
    - BARE：降级为 period_ms 周期回调（勿在 fn 内死循环）
*/
int os_thread_add(os_task_fn fn, const char *name, uint32_t stack_words,
                  uint32_t priority, uint32_t period_ms);

/*! 启动调度，正常不返回 */
void os_run(void);

/*! 延时毫秒（调度已启动时让出 CPU；未启动则忙等 SysTick） */
void os_delay_ms(uint32_t ms);

/*! 主动让出（无 RTOS 时为空操作或跑一轮到期任务） */
void os_yield(void);

/*! 单调毫秒节拍 */
uint32_t os_tick_ms(void);

/*! 调度器是否已运行 */
uint8_t os_is_running(void);

/*! SysTick_Handler 中调用（各 port 自己处理） */
void os_sys_tick_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
