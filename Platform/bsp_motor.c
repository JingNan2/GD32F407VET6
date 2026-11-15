#include "bsp_motor.h"
#include "board.h"
#include "systick.h"

#define MOTOR_TIMER_HZ      1000000UL

static uint32_t s_period = 0U;
static uint8_t  s_duty = 0U;
static uint8_t  s_running = 0U;
static uint8_t  s_vib_busy = 0U;
static uint32_t s_vib_start = 0U;
static uint32_t s_vib_ms = 0U;

static uint32_t duty_to_pulse(uint8_t duty)
{
    if(duty > 100U) {
        duty = 100U;
    }
    if((0U == duty) || (0U == s_period)) {
        return 0U;
    }
    return ((s_period + 1U) * duty) / 100U;
}

static void motor_apply_pulse(void)
{
    uint32_t pulse = 0U;

    if(0U != s_running) {
        pulse = duty_to_pulse(s_duty);
    }
    timer_channel_output_pulse_value_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, pulse);
}

void bsp_motor_init(void)
{
    timer_oc_parameter_struct oc_param;
    timer_parameter_struct timer_param;

    rcu_periph_clock_enable(BOARD_MOTOR_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_MOTOR_TIMER_CLK);
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);

    /* PB8 -> TIMER1_CH0 (AF1) */
    gpio_mode_set(BOARD_MOTOR_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BOARD_MOTOR_GPIO_PIN);
    gpio_output_options_set(BOARD_MOTOR_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_MOTOR_GPIO_PIN);
    gpio_af_set(BOARD_MOTOR_GPIO_PORT, BOARD_MOTOR_TIMER_AF, BOARD_MOTOR_GPIO_PIN);

    timer_deinit(BOARD_MOTOR_TIMER);

    /* 1MHz / 20kHz - 1 = 49 */
    s_period = (MOTOR_TIMER_HZ / BOARD_MOTOR_PWM_FREQ_HZ) - 1U;

    timer_param.prescaler         = (uint16_t)((SystemCoreClock / MOTOR_TIMER_HZ) - 1U);
    timer_param.alignedmode       = TIMER_COUNTER_EDGE;
    timer_param.counterdirection  = TIMER_COUNTER_UP;
    timer_param.period            = s_period;
    timer_param.clockdivision     = TIMER_CKDIV_DIV1;
    timer_param.repetitioncounter = 0U;
    timer_init(BOARD_MOTOR_TIMER, &timer_param);

    oc_param.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    oc_param.outputstate  = TIMER_CCX_ENABLE;
    oc_param.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    oc_param.outputnstate = TIMER_CCXN_DISABLE;
    oc_param.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    oc_param.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, &oc_param);

    timer_channel_output_mode_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, TIMER_OC_SHADOW_DISABLE);
    timer_auto_reload_shadow_enable(BOARD_MOTOR_TIMER);

    timer_channel_output_pulse_value_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, 0U);
    timer_enable(BOARD_MOTOR_TIMER);

    s_duty = 60U;
    s_running = 0U;
    s_vib_busy = 0U;
}

void bsp_motor_set_duty(uint8_t duty)
{
    if(duty > 100U) {
        duty = 100U;
    }
    s_duty = duty;
    motor_apply_pulse();
}

uint8_t bsp_motor_get_duty(void)
{
    return s_duty;
}

void bsp_motor_on(void)
{
    s_running = 1U;
    motor_apply_pulse();
}

void bsp_motor_off(void)
{
    s_running = 0U;
    timer_channel_output_pulse_value_config(BOARD_MOTOR_TIMER, BOARD_MOTOR_TIMER_CH, 0U);
}

void bsp_motor_vibrate(uint32_t ms)
{
    if(0U == ms) {
        bsp_motor_off();
        s_vib_busy = 0U;
        return;
    }

    s_vib_ms = ms;
    s_vib_start = sys_tick_get();
    s_vib_busy = 1U;
    bsp_motor_on();
}

void bsp_motor_task(void)
{
    if(0U == s_vib_busy) {
        return;
    }

    if(sys_tick_expired(s_vib_start, s_vib_ms)) {
        bsp_motor_off();
        s_vib_busy = 0U;
    }
}
