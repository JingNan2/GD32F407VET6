/*!
    \file    bsp_cst816.c
    \brief   CST816T 电容触摸（指令兼容参考驱动）
             软 I2C：PB6=SCL PB7=SDA；RST=PD7 INT=PB4
*/

#include "bsp_cst816.h"
#include "board.h"
#include "systick.h"
#include <stdio.h>

#define CST816_ADDR_7BIT    0x15U
#define REG_GESTURE         0x01U
#define REG_FINGER          0x02U
#define REG_XPOS_H          0x03U
#define REG_CHIP_ID         0xA7U
#define REG_FW_VER          0xA9U

static uint16_t s_x;
static uint16_t s_y;
static uint8_t  s_finger;
static uint8_t  s_ready;
static volatile uint8_t s_irq_flag;

static void i2c_delay(void)
{
    volatile uint32_t n = 40U;
    while(0U != n) {
        n--;
    }
}

static void scl_out(uint8_t level)
{
    if(0U != level) {
        gpio_bit_set(BOARD_TP_I2C_SCL_PORT, BOARD_TP_I2C_SCL_PIN);
    } else {
        gpio_bit_reset(BOARD_TP_I2C_SCL_PORT, BOARD_TP_I2C_SCL_PIN);
    }
}

static void sda_out(uint8_t level)
{
    gpio_mode_set(BOARD_TP_I2C_SDA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, BOARD_TP_I2C_SDA_PIN);
    if(0U != level) {
        gpio_bit_set(BOARD_TP_I2C_SDA_PORT, BOARD_TP_I2C_SDA_PIN);
    } else {
        gpio_bit_reset(BOARD_TP_I2C_SDA_PORT, BOARD_TP_I2C_SDA_PIN);
    }
}

static uint8_t sda_in(void)
{
    gpio_mode_set(BOARD_TP_I2C_SDA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_TP_I2C_SDA_PIN);
    return (RESET != gpio_input_bit_get(BOARD_TP_I2C_SDA_PORT, BOARD_TP_I2C_SDA_PIN)) ? 1U : 0U;
}

static void i2c_start(void)
{
    sda_out(1U);
    scl_out(1U);
    i2c_delay();
    sda_out(0U);
    i2c_delay();
    scl_out(0U);
}

static void i2c_stop(void)
{
    sda_out(0U);
    i2c_delay();
    scl_out(1U);
    i2c_delay();
    sda_out(1U);
    i2c_delay();
}

static uint8_t i2c_write_byte(uint8_t dat)
{
    uint8_t i;
    uint8_t ack;

    for(i = 0U; i < 8U; i++) {
        sda_out((0U != (dat & 0x80U)) ? 1U : 0U);
        dat <<= 1;
        i2c_delay();
        scl_out(1U);
        i2c_delay();
        scl_out(0U);
    }
    sda_in();
    i2c_delay();
    scl_out(1U);
    i2c_delay();
    ack = sda_in();
    scl_out(0U);
    return ack; /* 0=ACK */
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t dat = 0U;

    sda_in();
    for(i = 0U; i < 8U; i++) {
        dat <<= 1;
        scl_out(1U);
        i2c_delay();
        if(0U != sda_in()) {
            dat |= 0x01U;
        }
        scl_out(0U);
        i2c_delay();
    }
    sda_out((0U != ack) ? 1U : 0U); /* ack=1 → NACK */
    i2c_delay();
    scl_out(1U);
    i2c_delay();
    scl_out(0U);
    return dat;
}

static uint8_t cst816_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if((NULL == buf) || (0U == len)) {
        return CST816_ERROR;
    }

    i2c_start();
    if(0U != i2c_write_byte((uint8_t)(CST816_ADDR_7BIT << 1))) {
        i2c_stop();
        return CST816_ERROR;
    }
    if(0U != i2c_write_byte(reg)) {
        i2c_stop();
        return CST816_ERROR;
    }
    i2c_start();
    if(0U != i2c_write_byte((uint8_t)((CST816_ADDR_7BIT << 1) | 0x01U))) {
        i2c_stop();
        return CST816_ERROR;
    }
    for(i = 0U; i < len; i++) {
        buf[i] = i2c_read_byte((i + 1U) >= len ? 1U : 0U);
    }
    i2c_stop();
    return CST816_OK;
}

static void cst816_reset(void)
{
    gpio_bit_reset(BOARD_TP_RST_PORT, BOARD_TP_RST_PIN);
    delay_1ms(10U);
    gpio_bit_set(BOARD_TP_RST_PORT, BOARD_TP_RST_PIN);
    delay_1ms(50U);
}

static void cst816_read_point(void)
{
    uint8_t data[6];
    uint16_t x;
    uint16_t y;

    if(CST816_OK != cst816_read_reg(REG_GESTURE, &data[0], 1U)) {
        return;
    }
    if(CST816_OK != cst816_read_reg(REG_FINGER, &data[5], 1U)) {
        return;
    }
    if(CST816_OK != cst816_read_reg(REG_XPOS_H, &data[1], 4U)) {
        return;
    }

    x = (uint16_t)(((uint16_t)(data[1] & 0x0FU) << 8) | data[2]);
    y = (uint16_t)(((uint16_t)(data[3] & 0x0FU) << 8) | data[4]);
    s_finger = data[5];

    if((x < CST816_TOUCH_W) && (y < CST816_TOUCH_H)) {
        s_x = x;
        s_y = y;
    }
}

uint8_t bsp_cst816_init(void)
{
    s_ready = 0U;
    s_x = 0U;
    s_y = 0U;
    s_finger = 0U;
    s_irq_flag = 0U;

    rcu_periph_clock_enable(BOARD_TP_I2C_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_TP_RST_CLK);
    rcu_periph_clock_enable(BOARD_TP_INT_CLK);
    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_mode_set(BOARD_TP_I2C_SCL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, BOARD_TP_I2C_SCL_PIN);
    gpio_output_options_set(BOARD_TP_I2C_SCL_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, BOARD_TP_I2C_SCL_PIN);
    gpio_mode_set(BOARD_TP_I2C_SDA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, BOARD_TP_I2C_SDA_PIN);
    gpio_output_options_set(BOARD_TP_I2C_SDA_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, BOARD_TP_I2C_SDA_PIN);
    scl_out(1U);
    sda_out(1U);

    gpio_mode_set(BOARD_TP_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_TP_RST_PIN);
    gpio_output_options_set(BOARD_TP_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_TP_RST_PIN);
    gpio_bit_set(BOARD_TP_RST_PORT, BOARD_TP_RST_PIN);

    gpio_mode_set(BOARD_TP_INT_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_TP_INT_PIN);

    cst816_reset();
    delay_1ms(100U);

    {
        uint8_t id = 0U;
        uint8_t fw = 0U;
        if(CST816_OK != bsp_cst816_read_id(&id, &fw)) {
            return CST816_ERROR;
        }
        if((0U == id) || (0xFFU == id)) {
            return CST816_ERROR;
        }
    }

    nvic_irq_enable(BOARD_TP_EXTI_IRQn, 2U, 2U);
    syscfg_exti_line_config(BOARD_TP_EXTI_PORT_SRC, BOARD_TP_EXTI_PIN_SRC);
    exti_init(BOARD_TP_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(BOARD_TP_EXTI_LINE);
    exti_interrupt_enable(BOARD_TP_EXTI_LINE);

    s_ready = 1U;
    return CST816_OK;
}

uint8_t bsp_cst816_read_id(uint8_t *chip_id, uint8_t *fw_ver)
{
    uint8_t id = 0U;
    uint8_t fw = 0U;

    if(CST816_OK != cst816_read_reg(REG_CHIP_ID, &id, 1U)) {
        return CST816_ERROR;
    }
    if(CST816_OK != cst816_read_reg(REG_FW_VER, &fw, 1U)) {
        return CST816_ERROR;
    }
    if(NULL != chip_id) {
        *chip_id = id;
    }
    if(NULL != fw_ver) {
        *fw_ver = fw;
    }
    return CST816_OK;
}

void bsp_cst816_poll(void)
{
    if(0U == s_ready) {
        return;
    }
    /* 有中断或持续按下都刷新坐标 */
    if((0U != s_irq_flag) || (s_finger > 0U)) {
        s_irq_flag = 0U;
        cst816_read_point();
    }
}

void bsp_cst816_scan(void)
{
    if(0U == s_ready) {
        return;
    }
    s_irq_flag = 0U;
    cst816_read_point();
}

uint8_t bsp_cst816_is_pressed(void)
{
    return (s_finger > 0U) ? 1U : 0U;
}

void bsp_cst816_get_xy(uint16_t *x, uint16_t *y)
{
    if(NULL != x) {
        *x = s_x;
    }
    if(NULL != y) {
        *y = s_y;
    }
}

void bsp_cst816_exti_isr(void)
{
    if(RESET != exti_interrupt_flag_get(BOARD_TP_EXTI_LINE)) {
        exti_interrupt_flag_clear(BOARD_TP_EXTI_LINE);
        s_irq_flag = 1U;
    }
}
