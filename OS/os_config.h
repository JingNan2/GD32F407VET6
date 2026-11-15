#ifndef OS_CONFIG_H
#define OS_CONFIG_H

/*!
    \file    os_config.h
    \brief   选择 OS 后端；改这一处即可切换实现

    OS_PORT_FREERTOS : FreeRTOS 抢占式
    OS_PORT_BARE     : 前后台伪实时（SysTick + 主循环协作调度）
*/

#define OS_PORT_FREERTOS  1
#define OS_PORT_BARE      2

#ifndef OS_PORT
#define OS_PORT  OS_PORT_FREERTOS
#endif

/*
 * 切换说明：
 * 1) 把上面 OS_PORT 改成 OS_PORT_BARE 或 OS_PORT_FREERTOS，Rebuild
 * 2) 纯 BARE（省 Flash）：Keil 中 Exclude 掉 FreeRTOS 组，以及
 *    freertos_heap_ccm.c / freertos_hooks.c
 * 3) 应用只 #include "os.h"；周期任务用 os_task_add，长循环用 os_thread_add
 * 4) 新增其它 RTOS：仿 os_freertos.c 写 os_xxx.c，加 OS_PORT_XXX 即可
 */

#ifndef OS_TASK_MAX
#define OS_TASK_MAX  16U
#endif

#endif /* OS_CONFIG_H */
