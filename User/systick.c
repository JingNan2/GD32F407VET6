#include "gd32f4xx.h"
#include "systick.h"
#include "os.h"

static volatile uint32_t s_sys_tick_ms = 0U;
static volatile uint32_t s_delay_ms = 0U;

void systick_config(void)
{
    if(SysTick_Config(SystemCoreClock / 1000U)) {
        while(1) {
        }
    }
}

uint32_t systick_ms_get(void)
{
    return s_sys_tick_ms;
}

uint32_t sys_tick_get(void)
{
    return os_tick_ms();
}

uint8_t sys_tick_expired(uint32_t start, uint32_t ms)
{
    return (uint8_t)((uint32_t)(sys_tick_get() - start) >= ms);
}

void delay_1ms(uint32_t count)
{
    if(0U == count) {
        return;
    }

    if(0U != os_is_running()) {
        os_delay_ms(count);
        return;
    }

    /* 调度器未启动：阻塞等 SysTick */
    s_delay_ms = count;
    while(0U != s_delay_ms) {
    }
}

void systick_isr(void)
{
    s_sys_tick_ms++;
    if(0U != s_delay_ms) {
        s_delay_ms--;
    }
}
