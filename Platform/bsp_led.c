#include "bsp_led.h"
#include "board.h"

static const uint16_t s_led_pins[LED_NUM] = {
    BOARD_LED1_PIN,
    BOARD_LED2_PIN,
    BOARD_LED3_PIN,
    BOARD_LED4_PIN,
    BOARD_LED5_PIN,
    BOARD_LED6_PIN,
    BOARD_LED7_PIN,
    BOARD_LED8_PIN
};

static uint8_t s_led_mask = 0U;

void bsp_led_init(void)
{
    rcu_periph_clock_enable(BOARD_LED_SW_CLK);
    rcu_periph_clock_enable(BOARD_LED_GPIO_CLK);

    /* LED_SW：推挽输出，默认高=关闭 PNP 供电 */
    gpio_mode_set(BOARD_LED_SW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LED_SW_PIN);
    gpio_output_options_set(BOARD_LED_SW_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LED_SW_PIN);
    gpio_bit_set(BOARD_LED_SW_PORT, BOARD_LED_SW_PIN);

    /* LED1~8：推挽输出，默认高=灭 */
    gpio_mode_set(BOARD_LED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LED_PINS);
    gpio_output_options_set(BOARD_LED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LED_PINS);
    gpio_bit_set(BOARD_LED_GPIO_PORT, BOARD_LED_PINS);

    s_led_mask = 0U;

    /* 默认打开总供电，便于直接点灯 */
    bsp_led_power(1U);
}

void bsp_led_power(uint8_t enable)
{
    if(0U != enable) {
        gpio_bit_reset(BOARD_LED_SW_PORT, BOARD_LED_SW_PIN); /* 低电平打开 PNP */
    } else {
        gpio_bit_set(BOARD_LED_SW_PORT, BOARD_LED_SW_PIN);
    }
}

void bsp_led_on(uint8_t idx)
{
    if((idx < 1U) || (idx > LED_NUM)) {
        return;
    }
    idx -= 1U;
    gpio_bit_reset(BOARD_LED_GPIO_PORT, s_led_pins[idx]); /* 低电平亮 */
    s_led_mask |= (uint8_t)(1U << idx);
}

void bsp_led_off(uint8_t idx)
{
    if((idx < 1U) || (idx > LED_NUM)) {
        return;
    }
    idx -= 1U;
    gpio_bit_set(BOARD_LED_GPIO_PORT, s_led_pins[idx]);
    s_led_mask &= (uint8_t)(~(1U << idx));
}

void bsp_led_toggle(uint8_t idx)
{
    if((idx < 1U) || (idx > LED_NUM)) {
        return;
    }
    if(0U != (s_led_mask & (uint8_t)(1U << (idx - 1U)))) {
        bsp_led_off(idx);
    } else {
        bsp_led_on(idx);
    }
}

void bsp_led_write(uint8_t mask)
{
    uint8_t i;

    s_led_mask = mask;
    for(i = 0U; i < LED_NUM; i++) {
        if(0U != (mask & (uint8_t)(1U << i))) {
            gpio_bit_reset(BOARD_LED_GPIO_PORT, s_led_pins[i]);
        } else {
            gpio_bit_set(BOARD_LED_GPIO_PORT, s_led_pins[i]);
        }
    }
}

uint8_t bsp_led_read(void)
{
    return s_led_mask;
}

void bsp_led_all_off(void)
{
    gpio_bit_set(BOARD_LED_GPIO_PORT, BOARD_LED_PINS);
    s_led_mask = 0U;
}

void bsp_led_all_on(void)
{
    gpio_bit_reset(BOARD_LED_GPIO_PORT, BOARD_LED_PINS);
    s_led_mask = 0xFFU;
}
