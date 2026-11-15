#include "bsp_clock.h"

static FlagStatus s_lxtal_ready = RESET;

/*!
    \brief  SYSCLK 已由 SystemInit(168M_8M_HXTAL) 配置。
            这里只做：确认 PLL、补 CK48M、统一 TIMER 时钟倍频。
*/
ErrStatus bsp_clock_init(void)
{
    /* 确认已由 SystemInit 切到 PLLP（168M） */
    if(RCU_SCSS_PLLP != rcu_system_clock_source_get()) {
        return ERROR;
    }

    /* SDIO：CK48M ← PLL48M ← PLLQ(48M) */
    rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);
    rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);

    /* 所有 TIMER 初始化前统一倍频（官方 TIMER 例程常见做法） */
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);

    return SUCCESS;
}

ErrStatus bsp_lxtal_init(void)
{
    s_lxtal_ready = RESET;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    rcu_lxtal_drive_capability_config(RCU_LXTALDRI_HIGHER_DRIVE);

    rcu_osci_on(RCU_LXTAL);
    if(SUCCESS != rcu_osci_stab_wait(RCU_LXTAL)) {
        return ERROR;
    }

    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
    rcu_periph_clock_enable(RCU_RTC);

    s_lxtal_ready = SET;
    return SUCCESS;
}

FlagStatus bsp_lxtal_ready(void)
{
    return s_lxtal_ready;
}
