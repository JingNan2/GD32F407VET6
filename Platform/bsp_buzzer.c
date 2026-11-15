#include "bsp_buzzer.h"
#include "board.h"
#include "systick.h"

#define BUZZER_TIMER_HZ     1000000UL
#define BUZZER_FREQ_HZ      4000UL

static uint8_t  s_beep_busy = 0U;
static uint32_t s_beep_start = 0U;
static uint32_t s_beep_ms = 0U;
static uint32_t s_period = 0U;   /* ARR */
static uint8_t  s_volume = 80U;  /* 0~100 */
static uint8_t  s_pwm_on = 0U;

static uint32_t volume_to_pulse(uint8_t volume)
{
    uint32_t ticks;

    if(volume > 100U) {
        volume = 100U;
    }
    if((0U == volume) || (0U == s_period)) {
        return 0U;
    }

    ticks = s_period + 1U;
    /* 音量 100% -> 占空比 50%（方波最大声压） */
    return (ticks * volume) / 200U;
}

static void buzzer_apply_pulse(void)
{
    uint32_t pulse = 0U;

    if(0U != s_pwm_on) {
        pulse = volume_to_pulse(s_volume);
    }
    timer_channel_output_pulse_value_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, pulse);
}

static void buzzer_pwm_gpio_config(void)
{
    rcu_periph_clock_enable(BOARD_BUZZER_GPIO_CLK);

    gpio_mode_set(BOARD_BUZZER_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BOARD_BUZZER_GPIO_PIN);
    gpio_output_options_set(BOARD_BUZZER_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_BUZZER_GPIO_PIN);
    gpio_af_set(BOARD_BUZZER_GPIO_PORT, BOARD_BUZZER_TIMER_AF, BOARD_BUZZER_GPIO_PIN);
}

static void buzzer_pwm_timer_config(void)
{
    timer_oc_parameter_struct oc_param;
    timer_parameter_struct timer_param;

    rcu_periph_clock_enable(BOARD_BUZZER_TIMER_CLK);
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);

    timer_deinit(BOARD_BUZZER_TIMER);

    /* 1MHz / 4000Hz - 1 = 249 */
    s_period = (BUZZER_TIMER_HZ / BUZZER_FREQ_HZ) - 1U;

    timer_param.prescaler         = (uint16_t)((SystemCoreClock / BUZZER_TIMER_HZ) - 1U);
    timer_param.alignedmode       = TIMER_COUNTER_EDGE;
    timer_param.counterdirection  = TIMER_COUNTER_UP;
    timer_param.period            = s_period;
    timer_param.clockdivision     = TIMER_CKDIV_DIV1;
    timer_param.repetitioncounter = 0U;
    timer_init(BOARD_BUZZER_TIMER, &timer_param);

    oc_param.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    oc_param.outputstate  = TIMER_CCX_ENABLE;
    oc_param.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    oc_param.outputnstate = TIMER_CCXN_DISABLE;
    oc_param.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    oc_param.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, &oc_param);

    timer_channel_output_mode_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, TIMER_OC_SHADOW_DISABLE);
    timer_auto_reload_shadow_enable(BOARD_BUZZER_TIMER);

    timer_channel_output_pulse_value_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, 0U);
    timer_enable(BOARD_BUZZER_TIMER);
}

void bsp_buzzer_init(void)
{
    s_beep_busy = 0U;
    s_pwm_on = 0U;
    s_volume = 80U;

    buzzer_pwm_gpio_config();
    buzzer_pwm_timer_config();
    bsp_buzzer_off();
}

void bsp_buzzer_set_volume(uint8_t volume)
{
    if(volume > 100U) {
        volume = 100U;
    }
    s_volume = volume;
    buzzer_apply_pulse();
}

uint8_t bsp_buzzer_get_volume(void)
{
    return s_volume;
}

void bsp_buzzer_on(void)
{
    s_pwm_on = 1U;
    buzzer_apply_pulse();
}

void bsp_buzzer_off(void)
{
    s_pwm_on = 0U;
    timer_channel_output_pulse_value_config(BOARD_BUZZER_TIMER, BOARD_BUZZER_TIMER_CH, 0U);
}

void bsp_buzzer_beep(uint32_t ms)
{
    if(0U == ms) {
        bsp_buzzer_off();
        s_beep_busy = 0U;
        return;
    }

    s_beep_ms = ms;
    s_beep_start = sys_tick_get();
    s_beep_busy = 1U;
    bsp_buzzer_on();
}

void bsp_buzzer_task(void)
{
    if(0U == s_beep_busy) {
        return;
    }

    if(sys_tick_expired(s_beep_start, s_beep_ms)) {
        bsp_buzzer_off();
        s_beep_busy = 0U;
    }
}
