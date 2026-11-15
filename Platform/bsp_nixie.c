#include "bsp_nixie.h"
#include "board.h"

#define GET_BIT_VAL(byte, pos)  ((byte) & (1U << (pos)))
#define NOP_TIME()              do { __NOP(); __NOP(); } while(0)

#define NIXIE_DI(bit)   gpio_bit_write(BOARD_NIX_GPIO_PORT, BOARD_NIX_DI_PIN,  (bit) ? SET : RESET)
#define NIXIE_SCK(bit)  gpio_bit_write(BOARD_NIX_GPIO_PORT, BOARD_NIX_SCK_PIN, (bit) ? SET : RESET)
#define NIXIE_RCK(bit)  gpio_bit_write(BOARD_NIX_GPIO_PORT, BOARD_NIX_RCK_PIN, (bit) ? SET : RESET)

/*
 * 段码表（0=点亮）
 * 0~9 数字；10~19 数字+小数点；20 '.'；21 '-'；22~ AbCdEFHJLPqU
 */
static const uint8_t LED_TABLE[] = {
    0xC0U, 0xF9U, 0xA4U, 0xB0U, 0x99U, 0x92U, 0x82U, 0xF8U, 0x80U, 0x90U,
    0x64U, 0x79U, 0x24U, 0x30U, 0x19U, 0x12U, 0x02U, 0x78U, 0x00U, 0x10U,
    0x7FU, 0xBFU,
    0x88U, 0x83U, 0xC6U, 0xA1U, 0x86U, 0x8EU, 0x89U, 0xF1U, 0xC7U, 0x8CU, 0x98U, 0xC1U
};

#define LED_TABLE_SIZE  ((uint8_t)(sizeof(LED_TABLE) / sizeof(LED_TABLE[0])))

/* 缓冲存码表索引，0xFF=空白 */
static uint8_t s_disp[NIXIE_DIGIT_NUM];
static uint8_t s_scan_idx = 0U;

static void nixie_out(uint8_t dat)
{
    int8_t i;

    /* 8bit，先发出去的作为高位 */
    for(i = 7; i >= 0; i--) {
        NIXIE_DI(GET_BIT_VAL(dat, (uint8_t)i));
        NIXIE_SCK(0);
        NOP_TIME();
        NIXIE_SCK(1);
        NOP_TIME();
    }
}

static void rck_action(void)
{
    NIXIE_RCK(0);
    NOP_TIME();
    NIXIE_RCK(1);
    NOP_TIME();
}

/* 先发段码，再发位选，再锁存 */
static void nixie_show(uint8_t a_dat, uint8_t b_idx)
{
    nixie_out(a_dat);
    nixie_out(b_idx);
    rck_action();
}

static uint8_t table_lookup(uint8_t idx)
{
    if(NIXIE_IDX_BLANK == idx) {
        return 0xFFU; /* 全1=全灭 */
    }
    if(idx >= LED_TABLE_SIZE) {
        return 0xFFU;
    }
    return LED_TABLE[idx];
}

static void show_4(uint8_t base, uint16_t num, uint8_t leading_zero)
{
    uint8_t d[4];
    uint8_t i;
    uint8_t started = leading_zero;

    if(num > 9999U) {
        num = 9999U;
    }

    d[0] = (uint8_t)((num / 1000U) % 10U);
    d[1] = (uint8_t)((num / 100U) % 10U);
    d[2] = (uint8_t)((num / 10U) % 10U);
    d[3] = (uint8_t)(num % 10U);

    for(i = 0U; i < 4U; i++) {
        if((0U == started) && (0U == d[i]) && (i < 3U)) {
            s_disp[base + i] = NIXIE_IDX_BLANK;
        } else {
            started = 1U;
            s_disp[base + i] = d[i];
        }
    }
}

static void nixie_scan_timer_init(void)
{
    timer_parameter_struct timer_param;

    rcu_periph_clock_enable(BOARD_NIX_SCAN_TIMER_CLK);
    /* TIMER 倍频已在 bsp_clock_init() 统一配置，这里不再改全局 */

    timer_deinit(BOARD_NIX_SCAN_TIMER);

    /* 1MHz 计数，ARR=499 → 500us 中断一次切一位 */
    timer_param.prescaler         = (uint16_t)((SystemCoreClock / 1000000U) - 1U);
    timer_param.alignedmode       = TIMER_COUNTER_EDGE;
    timer_param.counterdirection  = TIMER_COUNTER_UP;
    timer_param.period            = 499U;
    timer_param.clockdivision     = TIMER_CKDIV_DIV1;
    timer_param.repetitioncounter = 0U;
    timer_init(BOARD_NIX_SCAN_TIMER, &timer_param);

    timer_interrupt_flag_clear(BOARD_NIX_SCAN_TIMER, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(BOARD_NIX_SCAN_TIMER, TIMER_INT_UP);

    /* 抢占优先级 2：低于 SDIO(0)，避免嵌套打乱 DMA */
    nvic_irq_enable(BOARD_NIX_SCAN_IRQn, 2U, 0U);
    timer_enable(BOARD_NIX_SCAN_TIMER);
}

void bsp_nixie_init(void)
{
    uint8_t i;

    rcu_periph_clock_enable(BOARD_NIX_GPIO_CLK);

    gpio_mode_set(BOARD_NIX_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_NIX_DI_PIN);
    gpio_output_options_set(BOARD_NIX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, BOARD_NIX_DI_PIN);

    gpio_mode_set(BOARD_NIX_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_NIX_SCK_PIN);
    gpio_output_options_set(BOARD_NIX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, BOARD_NIX_SCK_PIN);

    gpio_mode_set(BOARD_NIX_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_NIX_RCK_PIN);
    gpio_output_options_set(BOARD_NIX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, BOARD_NIX_RCK_PIN);

    NIXIE_DI(0);
    NIXIE_SCK(0);
    NIXIE_RCK(0);

    for(i = 0U; i < NIXIE_DIGIT_NUM; i++) {
        s_disp[i] = NIXIE_IDX_BLANK;
    }
    s_scan_idx = 0U;

    nixie_show(0xFFU, 0x00U);
    nixie_scan_timer_init();
}

void bsp_nixie_display(uint8_t num, uint8_t id)
{
    if(id >= NIXIE_DIGIT_NUM) {
        return;
    }
    /* 与参考 nixie_display(num, id) 一致 */
    nixie_show(table_lookup(num), (uint8_t)(1U << id));
}

void bsp_nixie_task(void)
{
    nixie_show(table_lookup(s_disp[s_scan_idx]), (uint8_t)(1U << s_scan_idx));

    s_scan_idx++;
    if(s_scan_idx >= NIXIE_DIGIT_NUM) {
        s_scan_idx = 0U;
    }
}

void bsp_nixie_set_digit(uint8_t idx, uint8_t table_idx)
{
    if(idx >= NIXIE_DIGIT_NUM) {
        return;
    }
    s_disp[idx] = table_idx;
}

void bsp_nixie_show_left(uint16_t num, uint8_t leading_zero)
{
    show_4(0U, num, leading_zero);
}

void bsp_nixie_show_right(uint16_t num, uint8_t leading_zero)
{
    show_4(4U, num, leading_zero);
}

void bsp_nixie_show_temp_x10(int16_t temp_x10)
{
    uint16_t t;
    uint8_t hundreds;
    uint8_t tens;
    uint8_t ones;
    uint8_t frac;

    if(temp_x10 < 0) {
        /* 负温：左4位显示 -XX. / -X.X 简化为绝对值带小数 */
        t = (uint16_t)(-temp_x10);
        if(t > 999U) {
            t = 999U;
        }
        s_disp[0] = NIXIE_IDX_MINUS;
        s_disp[1] = (uint8_t)((t / 100U) % 10U);
        s_disp[2] = (uint8_t)(NIXIE_IDX_DP0 + ((t / 10U) % 10U));
        s_disp[3] = (uint8_t)(t % 10U);
        if((0U == s_disp[1]) && (t < 100U)) {
            s_disp[1] = NIXIE_IDX_BLANK;
        }
        return;
    }

    t = (uint16_t)temp_x10;
    if(t > 9999U) {
        t = 9999U;
    }

    /* 274 → [ ][2][7.][4] */
    hundreds = (uint8_t)((t / 1000U) % 10U);
    tens     = (uint8_t)((t / 100U) % 10U);
    ones     = (uint8_t)((t / 10U) % 10U);
    frac     = (uint8_t)(t % 10U);

    s_disp[0] = (0U == hundreds) ? NIXIE_IDX_BLANK : hundreds;
    s_disp[1] = tens;
    if((0U == hundreds) && (0U == tens) && (t < 100U)) {
        /* 9.9 → [ ][ ][9.][9] */
        s_disp[1] = NIXIE_IDX_BLANK;
    }
    s_disp[2] = (uint8_t)(NIXIE_IDX_DP0 + ones);
    s_disp[3] = frac;
}

void bsp_nixie_clear(void)
{
    uint8_t i;

    for(i = 0U; i < NIXIE_DIGIT_NUM; i++) {
        s_disp[i] = NIXIE_IDX_BLANK;
    }
}
