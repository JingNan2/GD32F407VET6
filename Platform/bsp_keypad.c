#include "bsp_keypad.h"
#include "board.h"

#define KEYPAD_DEBOUNCE_CNT     2U

static const uint16_t s_row_pins[4] = {
    BOARD_KEY_ROW1_PIN,
    BOARD_KEY_ROW2_PIN,
    BOARD_KEY_ROW3_PIN,
    BOARD_KEY_ROW4_PIN
};

static const uint16_t s_col_pins[4] = {
    BOARD_KEY_COL1_PIN,
    BOARD_KEY_COL2_PIN,
    BOARD_KEY_COL3_PIN,
    BOARD_KEY_COL4_PIN
};

static uint8_t s_raw_key = KEYPAD_KEY_NONE;
static uint8_t s_stable_key = KEYPAD_KEY_NONE;
static uint8_t s_debounce = 0U;
static uint8_t s_evt_key = KEYPAD_KEY_NONE;
static keypad_event_t s_evt = KEYPAD_EVT_NONE;

static uint8_t keypad_scan_raw(void)
{
    uint8_t row;
    uint8_t col;
    uint8_t key = KEYPAD_KEY_NONE;

    /* 所有行先拉高 */
    gpio_bit_set(BOARD_KEY_GPIO_PORT, BOARD_KEY_ROW_PINS);

    for(row = 0U; row < 4U; row++) {
        gpio_bit_reset(BOARD_KEY_GPIO_PORT, s_row_pins[row]);

        /* 短暂延时，等待电平稳定 */
        {
            volatile uint32_t d;
            for(d = 0U; d < 50U; d++) {
            }
        }

        for(col = 0U; col < 4U; col++) {
            if(RESET == gpio_input_bit_get(BOARD_KEY_GPIO_PORT, s_col_pins[col])) {
                /* S1=1 ... S16=16：key = row*4 + col + 1 */
                key = (uint8_t)(row * 4U + col + 1U);
                gpio_bit_set(BOARD_KEY_GPIO_PORT, s_row_pins[row]);
                return key;
            }
        }

        gpio_bit_set(BOARD_KEY_GPIO_PORT, s_row_pins[row]);
    }

    return KEYPAD_KEY_NONE;
}

void bsp_keypad_init(void)
{
    rcu_periph_clock_enable(BOARD_KEY_GPIO_CLK);

    /* 行：推挽输出，默认高 */
    gpio_mode_set(BOARD_KEY_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_KEY_ROW_PINS);
    gpio_output_options_set(BOARD_KEY_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_KEY_ROW_PINS);
    gpio_bit_set(BOARD_KEY_GPIO_PORT, BOARD_KEY_ROW_PINS);

    /* 列：输入上拉（原理图无外置电阻） */
    gpio_mode_set(BOARD_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_KEY_COL_PINS);

    s_raw_key = KEYPAD_KEY_NONE;
    s_stable_key = KEYPAD_KEY_NONE;
    s_debounce = 0U;
    s_evt_key = KEYPAD_KEY_NONE;
    s_evt = KEYPAD_EVT_NONE;
}

void bsp_keypad_task(void)
{
    uint8_t key = keypad_scan_raw();

    if(key == s_raw_key) {
        if(s_debounce < KEYPAD_DEBOUNCE_CNT) {
            s_debounce++;
        }
    } else {
        s_raw_key = key;
        s_debounce = 0U;
    }

    if(s_debounce < KEYPAD_DEBOUNCE_CNT) {
        return;
    }

    if(key == s_stable_key) {
        return;
    }

    /* 状态变化：先报松开，再报按下 */
    if(KEYPAD_KEY_NONE != s_stable_key) {
        s_evt = KEYPAD_EVT_RELEASE;
        s_evt_key = s_stable_key;
    }

    s_stable_key = key;

    if(KEYPAD_KEY_NONE != key) {
        s_evt = KEYPAD_EVT_PRESS;
        s_evt_key = key;
    }
}

uint8_t bsp_keypad_get_key(void)
{
    return s_stable_key;
}

keypad_event_t bsp_keypad_get_event(uint8_t *key)
{
    keypad_event_t evt = s_evt;

    if(KEYPAD_EVT_NONE == evt) {
        if(0 != key) {
            *key = KEYPAD_KEY_NONE;
        }
        return KEYPAD_EVT_NONE;
    }

    if(0 != key) {
        *key = s_evt_key;
    }

    s_evt = KEYPAD_EVT_NONE;
    s_evt_key = KEYPAD_KEY_NONE;
    return evt;
}
