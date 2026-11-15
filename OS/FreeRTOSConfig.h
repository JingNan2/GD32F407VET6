#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ARMCLANG(AC6) 使用 GCC CM4F port 时需具备该宏 */
#if defined(__ARMCC_VERSION) && !defined(__VFP_FP__)
#define __VFP_FP__
#endif

#include <stdint.h>

extern uint32_t SystemCoreClock;

/******************************************************************************/
/* 与 CMSIS 异常名对接（SVC / PendSV 由 FreeRTOS port 提供） */
/******************************************************************************/
#define vPortSVCHandler       SVC_Handler
#define xPortPendSVHandler    PendSV_Handler
/* SysTick 仍由 gd32f4xx_it.c 转发，便于启动前 delay_1ms */

/******************************************************************************/
/* 时钟与节拍 */
/******************************************************************************/
#define configCPU_CLOCK_HZ                    (SystemCoreClock)
#define configTICK_RATE_HZ                    ((TickType_t)1000)
#define configUSE_16_BIT_TICKS                0

/******************************************************************************/
/* 调度 */
/******************************************************************************/
#define configUSE_PREEMPTION                  1
#define configUSE_TIME_SLICING                1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE               0
#define configMAX_PRIORITIES                  8
#define configMINIMAL_STACK_SIZE              ((uint16_t)128)
#define configMAX_TASK_NAME_LEN               16
#define configIDLE_SHOULD_YIELD               1

/******************************************************************************/
/* 内存：heap_4，约 48KB */
/******************************************************************************/
#define configSUPPORT_STATIC_ALLOCATION       0
#define configSUPPORT_DYNAMIC_ALLOCATION      1
#define configTOTAL_HEAP_SIZE                 ((size_t)(40 * 1024))
/* 堆放到 CCM，主 SRAM 留给 LVGL/DMA */
#define configAPPLICATION_ALLOCATED_HEAP      1

/******************************************************************************/
/* 钩子 / 检查 */
/******************************************************************************/
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configCHECK_FOR_STACK_OVERFLOW        2
#define configUSE_MALLOC_FAILED_HOOK          1
#define configGENERATE_RUN_TIME_STATS         0
#define configUSE_TRACE_FACILITY              0
#define configUSE_STATS_FORMATTING_FUNCTIONS  0

/******************************************************************************/
/* 同步原语 */
/******************************************************************************/
#define configUSE_MUTEXES                     1
#define configUSE_RECURSIVE_MUTEXES           1
#define configUSE_COUNTING_SEMAPHORES         1
#define configUSE_QUEUE_SETS                  0
#define configQUEUE_REGISTRY_SIZE             0
#define configUSE_TASK_NOTIFICATIONS          1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1

/******************************************************************************/
/* 软件定时器 */
/******************************************************************************/
#define configUSE_TIMERS                      1
#define configTIMER_TASK_PRIORITY             (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH              8
#define configTIMER_TASK_STACK_DEPTH          256

/******************************************************************************/
/* 中断优先级（4 位抢占：0 最高 ~ 15 最低） */
/******************************************************************************/
#define configPRIO_BITS                       4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/******************************************************************************/
/* 可选 API */
/******************************************************************************/
#define INCLUDE_vTaskPrioritySet              1
#define INCLUDE_uxTaskPriorityGet             1
#define INCLUDE_vTaskDelete                   1
#define INCLUDE_vTaskSuspend                  1
#define INCLUDE_vTaskDelayUntil               1
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_xTaskGetSchedulerState        1
#define INCLUDE_xTaskGetCurrentTaskHandle     1
#define INCLUDE_uxTaskGetStackHighWaterMark   1
#define INCLUDE_xTaskGetIdleTaskHandle        0
#define INCLUDE_eTaskGetState                 1
#define INCLUDE_xTimerPendFunctionCall        1
#define INCLUDE_xTaskAbortDelay               0
#define INCLUDE_xTaskGetHandle                0

#define configASSERT(x)                       if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;){} }

#endif /* FREERTOS_CONFIG_H */
