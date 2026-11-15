/*!
    \file    bsp_usb_hw.c
    \brief   USBFS 硬件：PA11/PA12
             延时改为空循环（不用 TIMER），避免中断风暴饿死 OS/数码管
*/

#include "drv_usb_hw.h"

void usb_rcu_config(void)
{
    rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);
    rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);
    rcu_periph_clock_enable(RCU_USBFS);
}

void usb_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_GPIOA);

    /* USBFS_DM(PA11) USBFS_DP(PA12) AF10 */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11 | GPIO_PIN_12);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11 | GPIO_PIN_12);
    gpio_af_set(GPIOA, GPIO_AF_10, GPIO_PIN_11 | GPIO_PIN_12);
}

void usb_intr_config(void)
{
    /* 优先级低于数码管(2) / SDIO(0)，避免 USB 占满 CPU */
    nvic_irq_enable(USBFS_IRQn, 3U, 0U);
}

void usb_timer_init(void)
{
    /* 空实现：udelay/mdelay 用忙等，不占用 TIMER */
}

void usb_udelay(const uint32_t usec)
{
    uint32_t count = usec * (SystemCoreClock / 1000000U / 5U);
    while(count--) {
        __NOP();
    }
}

void usb_mdelay(const uint32_t msec)
{
    uint32_t i;
    for(i = 0U; i < msec; i++) {
        usb_udelay(1000U);
    }
}

void system_clk_config_stop(void)
{
}

void usb_timer_irq(void)
{
    /* 兼容旧声明，无定时器中断 */
}
