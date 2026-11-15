/*!
    \file    os_bare.c
    \brief   OS 抽象 — 前后台伪实时（协作式主循环）
*/

#include "os_config.h"

#if (OS_PORT == OS_PORT_BARE)

#include "os.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    os_task_fn fn;
    uint32_t   period_ms;
    uint32_t   last_ms;
    uint8_t    used;
    char       name[8];
} os_slot_t;

static os_slot_t s_slots[OS_TASK_MAX];
static uint8_t   s_num = 0U;
static uint8_t   s_running = 0U;
static uint8_t   s_in_sched = 0U;

extern uint32_t systick_ms_get(void);
extern void systick_isr(void);

static void bare_run_due(void)
{
    uint8_t i;
    uint32_t now;

    if(0U != s_in_sched) {
        return;
    }
    s_in_sched = 1U;
    now = systick_ms_get();
    for(i = 0U; i < s_num; i++) {
        if(0U == s_slots[i].used) {
            continue;
        }
        if((uint32_t)(now - s_slots[i].last_ms) >= s_slots[i].period_ms) {
            s_slots[i].last_ms = now;
            if(NULL != s_slots[i].fn) {
                s_slots[i].fn();
            }
        }
    }
    s_in_sched = 0U;
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
    s_slots[s_num].period_ms = (0U == period_ms) ? 1U : period_ms;
    s_slots[s_num].last_ms = 0U;
    s_slots[s_num].used = 1U;
    (void)snprintf(s_slots[s_num].name, sizeof(s_slots[s_num].name), "t%u", (unsigned)s_num);
    s_num++;
    return 0;
}

int os_thread_add(os_task_fn fn, const char *name, uint32_t stack_words,
                  uint32_t priority, uint32_t period_ms)
{
    (void)stack_words;
    (void)priority;
    /* 裸机：线程降级为周期回调；fn 禁止死循环 */
    if(0U == period_ms) {
        period_ms = 5U;
    }
    if(0 != os_task_add(fn, period_ms)) {
        return -1;
    }
    if(NULL != name) {
        (void)strncpy(s_slots[s_num - 1U].name, name, sizeof(s_slots[s_num - 1U].name) - 1U);
    }
    return 0;
}

void os_run(void)
{
    uint8_t i;
    uint32_t now = systick_ms_get();

    printf("[OS] BARE coop, slots=%u\r\n", (unsigned)s_num);
    for(i = 0U; i < s_num; i++) {
        s_slots[i].last_ms = now;
    }
    s_running = 1U;
    for(;;) {
        bare_run_due();
    }
}

void os_delay_ms(uint32_t ms)
{
    uint32_t start;

    if(0U == ms) {
        return;
    }
    start = systick_ms_get();
    while((uint32_t)(systick_ms_get() - start) < ms) {
        if(0U != s_running) {
            bare_run_due(); /* 延时期间仍跑到期任务 */
        }
    }
}

void os_yield(void)
{
    if(0U != s_running) {
        bare_run_due();
    }
}

uint32_t os_tick_ms(void)
{
    return systick_ms_get();
}

uint8_t os_is_running(void)
{
    return s_running;
}

void os_sys_tick_handler(void)
{
    systick_isr();
}

#endif /* OS_PORT_BARE */
