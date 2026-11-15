/*!
    \file    freertos_hooks.c
    \brief   FreeRTOS 钩子（与 OS 后端解耦，便于 BARE 时仍链接 FreeRTOS 内核）

    切纯 BARE 且不编 FreeRTOS 时，从工程中排除本文件。
*/

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    printf("[OS] malloc failed\r\n");
    for(;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    printf("[OS] stack overflow: %s\r\n", (NULL != pcTaskName) ? pcTaskName : "?");
    for(;;) {
    }
}
