#include "bsp_button.h"
#include "board.h"

#define BTN_DEBOUNCE_CNT    2U

typedef struct {
    uint16_t pin;
    uint8_t  id;
} btn_map_t;

static const btn_map_t s_btn_map[BTN_NUM] = {
    {BOARD_BTN_S17_PIN, BTN_S17},
    {BOARD_BTN_S18_PIN, BTN_S18},
    {BOARD_BTN_S19_PIN, BTN_S19},
    {BOARD_BTN_S20_PIN, BTN_S20}
};

static uint8_t s_raw[BTN_NUM];
static uint8_t s_stable[BTN_NUM];
static uint8_t s_debounce[BTN_NUM];
static uint8_t s_evt_id = BTN_NONE;
static btn_event_t s_evt = BTN_EVT_NONE;

void bsp_button_init(void)
{
    uint8_t i;

    rcu_periph_clock_enable(BOARD_BTN_GPIO_CLK);

    /* 输入上拉；按下经 220R 到 GND -> 读到低电平 */
    gpio_mode_set(BOARD_BTN_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_BTN_PINS);

    for(i = 0U; i < BTN_NUM; i++) {
        s_raw[i] = 0U;
        s_stable[i] = 0U;
        s_debounce[i] = 0U;
    }
    s_evt_id = BTN_NONE;
    s_evt = BTN_EVT_NONE;
}

void bsp_button_task(void)
{
    uint8_t i;
    uint8_t pressed;

    for(i = 0U; i < BTN_NUM; i++) {
        pressed = (RESET == gpio_input_bit_get(BOARD_BTN_GPIO_PORT, s_btn_map[i].pin)) ? 1U : 0U;

        if(pressed == s_raw[i]) {
            if(s_debounce[i] < BTN_DEBOUNCE_CNT) {
                s_debounce[i]++;
            }
        } else {
            s_raw[i] = pressed;
            s_debounce[i] = 0U;
        }

        if(s_debounce[i] < BTN_DEBOUNCE_CNT) {
            continue;
        }

        if(pressed == s_stable[i]) {
            continue;
        }

        s_stable[i] = pressed;
        s_evt_id = s_btn_map[i].id;
        s_evt = (0U != pressed) ? BTN_EVT_PRESS : BTN_EVT_RELEASE;
    }
}

uint8_t bsp_button_is_pressed(uint8_t id)
{
    uint8_t i;

    for(i = 0U; i < BTN_NUM; i++) {
        if(s_btn_map[i].id == id) {
            return s_stable[i];
        }
    }
    return 0U;
}

btn_event_t bsp_button_get_event(uint8_t *id)
{
    btn_event_t evt = s_evt;

    if(BTN_EVT_NONE == evt) {
        if(0 != id) {
            *id = BTN_NONE;
        }
        return BTN_EVT_NONE;
    }

    if(0 != id) {
        *id = s_evt_id;
    }

    s_evt = BTN_EVT_NONE;
    s_evt_id = BTN_NONE;
    return evt;
}
