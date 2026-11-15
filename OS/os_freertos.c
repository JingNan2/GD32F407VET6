/*!
    \file    os_freertos.c
    \brief   OS 抽象 — FreeRTOS 后端
*/

#include "os_config.h"

#if (OS_PORT == OS_PORT_FREERTOS)

#include "os.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    os_task_fn   fn;
    uint32_t     period_ms;
    uint8_t      used;
    uint8_t      is_thread; /* 1: fn 自带循环；0: 周期回调 */
    uint16_t     stack_words;
    UBaseType_t  prio;
    char         name[8];
    TaskHandle_t handle;
} os_slot_t;

static os_slot_t s_slots[OS_TASK_MAX];
static uint8_t   s_num = 0U;
static uint8_t   s_running = 0U;

static void os_periodic_entry(void *param)
{
    os_slot_t *t = (os_slot_t *)param;
    TickType_t last = xTaskGetTickCount();
    TickType_t period = pdMS_TO_TICKS((0U == t->period_ms) ? 1U : t->period_ms);

    if(0U == period) {
        period = 1U;
    }
    for(;;) {
        if(NULL != t->fn) {
            t->fn();
        }
        vTaskDelayUntil(&last, period);
    }
}

static void os_thread_entry(void *param)
{
    os_slot_t *t = (os_slot_t *)param;
    if(NULL != t->fn) {
        t->fn();
    }
    vTaskDelete(NULL);
}

void os_init(void)
{
    uint8_t i;
    s_num = 0U;
    s_running = 0U;
    for(i = 0U; i < OS_TASK_MAX; i++) {
        memset(&s_slots[i], 0, sizeof(s_slots[i]));
    }
}

int os_task_add(os_task_fn fn, uint32_t period_ms)
{
    if((NULL == fn) || (s_num >= OS_TASK_MAX)) {
        return -1;
    }
    s_slots[s_num].fn = fn;
    s_slots[s_num].period_ms = period_ms;
    s_slots[s_num].used = 1U;
    s_slots[s_num].is_thread = 0U;
    if(period_ms <= 1U) {
        s_slots[s_num].prio = 4U;
        s_slots[s_num].stack_words = 256U;
    } else if(period_ms <= 20U) {
        s_slots[s_num].prio = 3U;
        s_slots[s_num].stack_words = 384U;
    } else {
        s_slots[s_num].prio = 2U;
        s_slots[s_num].stack_words = 256U;
    }
    (void)snprintf(s_slots[s_num].name, sizeof(s_slots[s_num].name), "t%u", (unsigned)s_num);
    s_num++;
    return 0;
}

int os_thread_add(os_task_fn fn, const char *name, uint32_t stack_words,
                  uint32_t priority, uint32_t period_ms)
{
    (void)period_ms;
    if((NULL == fn) || (s_num >= OS_TASK_MAX)) {
        return -1;
    }
    s_slots[s_num].fn = fn;
    s_slots[s_num].period_ms = 0U;
    s_slots[s_num].used = 1U;
    s_slots[s_num].is_thread = 1U;
    s_slots[s_num].stack_words = (uint16_t)((stack_words < 128U) ? 128U : stack_words);
    s_slots[s_num].prio = (UBaseType_t)priority;
    if(NULL != name) {
        (void)strncpy(s_slots[s_num].name, name, sizeof(s_slots[s_num].name) - 1U);
    } else {
        (void)snprintf(s_slots[s_num].name, sizeof(s_slots[s_num].name), "th%u", (unsigned)s_num);
    }
    s_num++;
    return 0;
}

void os_run(void)
{
    uint8_t i;
    BaseType_t ret;

    printf("[OS] FreeRTOS %s, slots=%u\r\n",
           tskKERNEL_VERSION_NUMBER, (unsigned)s_num);

    for(i = 0U; i < s_num; i++) {
        if(0U == s_slots[i].used) {
            continue;
        }
        if(0U != s_slots[i].is_thread) {
            ret = xTaskCreate(os_thread_entry, s_slots[i].name,
                              s_slots[i].stack_words, &s_slots[i],
                              s_slots[i].prio, &s_slots[i].handle);
        } else {
            ret = xTaskCreate(os_periodic_entry, s_slots[i].name,
                              s_slots[i].stack_words, &s_slots[i],
                              s_slots[i].prio, &s_slots[i].handle);
        }
        if(pdPASS != ret) {
            printf("[OS] create fail %s\r\n", s_slots[i].name);
        }
    }

    s_running = 1U;
    vTaskStartScheduler();
    printf("[OS] scheduler exited\r\n");
    for(;;) {
    }
}

void os_delay_ms(uint32_t ms)
{
    if(0U == ms) {
        return;
    }
    if(taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        /* 启动前：由 systick 忙等路径，这里用轮询 tick */
        uint32_t start = os_tick_ms();
        while((uint32_t)(os_tick_ms() - start) < ms) {
        }
    }
}

void os_yield(void)
{
    if(taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
        taskYIELD();
    }
}

uint32_t os_tick_ms(void)
{
    if(taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()) {
        return (uint32_t)xTaskGetTickCount();
    }
    /* 与 systick_isr 共享：启动前用外部递增的毫秒（见 systick.c） */
    extern uint32_t systick_ms_get(void);
    return systick_ms_get();
}

uint8_t os_is_running(void)
{
    return (uint8_t)(taskSCHEDULER_RUNNING == xTaskGetSchedulerState());
}

void os_sys_tick_handler(void)
{
    if(taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()) {
        extern void xPortSysTickHandler(void);
        xPortSysTickHandler();
    } else {
        extern void systick_isr(void);
        systick_isr();
    }
}

#endif /* OS_PORT_FREERTOS */
