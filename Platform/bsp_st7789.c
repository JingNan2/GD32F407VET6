/*!
    \file    bsp_st7789.c
    \brief   ST7789 240x280
             SPI0 + DMA 经 bsp_spi0_bus 与 Flash 分时（Flash 优先，LCD 行间让出）
*/

#include "bsp_st7789.h"
#include "bsp_spi0_bus.h"
#include "board.h"
#include "systick.h"

#define ST7789_SLPOUT   0x11U
#define ST7789_INVON    0x21U
#define ST7789_DISPON   0x29U
#define ST7789_CASET    0x2AU
#define ST7789_RASET    0x2BU
#define ST7789_RAMWR    0x2CU
#define ST7789_MADCTL   0x36U
#define ST7789_COLMOD   0x3AU
#define ST7789_NORON    0x13U

static uint8_t s_line_buf[ST7789_WIDTH * 2U];

static void lcd_cs_low(void)
{
    gpio_bit_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);
    gpio_bit_reset(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
}

static void lcd_cs_high(void)
{
    gpio_bit_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
}

static void lcd_dc_cmd(void)
{
    gpio_bit_reset(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
}

static void lcd_dc_data(void)
{
    gpio_bit_set(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
}

static uint8_t lcd_begin(void)
{
    return bsp_spi0_bus_acquire(SPI0_BUS_LCD);
}

static void lcd_end(void)
{
    lcd_cs_high();
    bsp_spi0_bus_release(SPI0_BUS_LCD);
}

static void spi_write_byte(uint8_t dat)
{
    (void)bsp_spi0_bus_write(&dat, 1U);
}

static void write_cmd(uint8_t cmd)
{
    lcd_cs_low();
    lcd_dc_cmd();
    spi_write_byte(cmd);
    lcd_cs_high();
}

static void write_data(uint8_t dat)
{
    lcd_cs_low();
    lcd_dc_data();
    spi_write_byte(dat);
    lcd_cs_high();
}

static void write_data_buf(const uint8_t *buf, uint32_t len)
{
    lcd_cs_low();
    lcd_dc_data();
    (void)bsp_spi0_bus_write(buf, len);
    lcd_cs_high();
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t xs = (uint16_t)(x0 + ST7789_X_SHIFT);
    uint16_t xe = (uint16_t)(x1 + ST7789_X_SHIFT);
    uint16_t ys = (uint16_t)(y0 + ST7789_Y_SHIFT);
    uint16_t ye = (uint16_t)(y1 + ST7789_Y_SHIFT);
    uint8_t caset[4];
    uint8_t raset[4];

    caset[0] = (uint8_t)(xs >> 8);
    caset[1] = (uint8_t)(xs & 0xFFU);
    caset[2] = (uint8_t)(xe >> 8);
    caset[3] = (uint8_t)(xe & 0xFFU);

    raset[0] = (uint8_t)(ys >> 8);
    raset[1] = (uint8_t)(ys & 0xFFU);
    raset[2] = (uint8_t)(ye >> 8);
    raset[3] = (uint8_t)(ye & 0xFFU);

    write_cmd(ST7789_CASET);
    write_data_buf(caset, 4U);
    write_cmd(ST7789_RASET);
    write_data_buf(raset, 4U);
    write_cmd(ST7789_RAMWR);
}

static void gpio_ctrl_init(void)
{
    rcu_periph_clock_enable(BOARD_LCD_CS_CLK);
    rcu_periph_clock_enable(BOARD_LCD_DC_CLK);
    rcu_periph_clock_enable(BOARD_LCD_RST_CLK);
    rcu_periph_clock_enable(BOARD_LCD_BLK_CLK);

    gpio_mode_set(BOARD_LCD_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LCD_CS_PIN);
    gpio_output_options_set(BOARD_LCD_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LCD_CS_PIN);
    lcd_cs_high();

    gpio_mode_set(BOARD_LCD_DC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LCD_DC_PIN);
    gpio_output_options_set(BOARD_LCD_DC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LCD_DC_PIN);
    gpio_bit_set(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);

    gpio_mode_set(BOARD_LCD_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LCD_RST_PIN);
    gpio_output_options_set(BOARD_LCD_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LCD_RST_PIN);
    gpio_bit_set(BOARD_LCD_RST_PORT, BOARD_LCD_RST_PIN);

    gpio_mode_set(BOARD_LCD_BLK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_LCD_BLK_PIN);
    gpio_output_options_set(BOARD_LCD_BLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LCD_BLK_PIN);
    gpio_bit_set(BOARD_LCD_BLK_PORT, BOARD_LCD_BLK_PIN);
}

void bsp_st7789_backlight(uint8_t on)
{
    if(0U != on) {
        gpio_bit_set(BOARD_LCD_BLK_PORT, BOARD_LCD_BLK_PIN);
    } else {
        gpio_bit_reset(BOARD_LCD_BLK_PORT, BOARD_LCD_BLK_PIN);
    }
}

void bsp_st7789_init(void)
{
    bsp_spi0_bus_init();
    gpio_ctrl_init();

    delay_1ms(25U);
    gpio_bit_reset(BOARD_LCD_RST_PORT, BOARD_LCD_RST_PIN);
    delay_1ms(25U);
    gpio_bit_set(BOARD_LCD_RST_PORT, BOARD_LCD_RST_PIN);
    delay_1ms(50U);
    bsp_st7789_backlight(1U);

    (void)lcd_begin();
    write_cmd(ST7789_SLPOUT);
    delay_1ms(120U);
    write_cmd(ST7789_MADCTL);
    write_data(0x00U);
    write_cmd(ST7789_COLMOD);
    write_data(0x55U); /* 16bit/pixel RGB565 */
    /* RAMCTRL：RGB565→面板，避免发色异常 */
    write_cmd(0xB0U);
    write_data(0x00U);
    write_data(0xC0U);

    write_cmd(0xB2U);
    {
        uint8_t d[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
        write_data_buf(d, sizeof(d));
    }
    write_cmd(0xB7U);
    write_data(0x72U);
    write_cmd(0xBBU);
    write_data(0x3DU);
    write_cmd(0xC0U);
    write_data(0x2CU);
    write_cmd(0xC2U);
    write_data(0x01U);
    write_cmd(0xC3U);
    write_data(0x19U);
    write_cmd(0xC4U);
    write_data(0x20U);
    write_cmd(0xC6U);
    write_data(0x01U);
    write_cmd(0xD0U);
    write_data(0xA4U);
    write_data(0xA1U);

    write_cmd(0xE0U);
    {
        uint8_t d[] = {0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x05U, 0x31U, 0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};
        write_data_buf(d, sizeof(d));
    }
    write_cmd(0xE1U);
    {
        uint8_t d[] = {0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x15U, 0x31U, 0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};
        write_data_buf(d, sizeof(d));
    }

    write_cmd(ST7789_INVON);
    write_cmd(ST7789_SLPOUT);
    write_cmd(ST7789_NORON);
    write_cmd(ST7789_DISPON);
    lcd_end();
    delay_1ms(50U);
}

void bsp_st7789_fill(uint16_t color)
{
    bsp_st7789_fill_rect(0U, 0U, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void bsp_st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t row;
    uint16_t col;
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFFU);
    uint32_t line_bytes;

    if((0U == w) || (0U == h) || (x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) {
        return;
    }
    if((x + w) > ST7789_WIDTH) {
        w = (uint16_t)(ST7789_WIDTH - x);
    }
    if((y + h) > ST7789_HEIGHT) {
        h = (uint16_t)(ST7789_HEIGHT - y);
    }

    for(col = 0U; col < w; col++) {
        s_line_buf[col * 2U] = hi;
        s_line_buf[col * 2U + 1U] = lo;
    }
    line_bytes = (uint32_t)w * 2U;

    if(SPI0_BUS_OK != lcd_begin()) {
        return;
    }
    set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    lcd_cs_low();
    lcd_dc_data();
    for(row = 0U; row < h; row++) {
        if(0U != bsp_spi0_bus_flash_pending()) {
            lcd_cs_high();
            lcd_end();
            (void)lcd_begin();
            set_window(x, (uint16_t)(y + row), (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
            lcd_cs_low();
            lcd_dc_data();
        }
        (void)bsp_spi0_bus_write(s_line_buf, line_bytes);
    }
    lcd_cs_high();
    lcd_end();
}

void bsp_st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) {
        return;
    }
    if(SPI0_BUS_OK != lcd_begin()) {
        return;
    }
    set_window(x, y, x, y);
    write_data((uint8_t)(color >> 8));
    write_data((uint8_t)(color & 0xFFU));
    lcd_end();
}

void bsp_st7789_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    bsp_st7789_fill_rect(x, y, len, 1U, color);
}

void bsp_st7789_draw_vline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    bsp_st7789_fill_rect(x, y, 1U, len, color);
}

void bsp_st7789_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px)
{
    uint32_t nbytes;

    if((NULL == px) || (0U == w) || (0U == h) || (x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) {
        return;
    }
    if((x + w) > ST7789_WIDTH) {
        w = (uint16_t)(ST7789_WIDTH - x);
    }
    if((y + h) > ST7789_HEIGHT) {
        h = (uint16_t)(ST7789_HEIGHT - y);
    }

    nbytes = (uint32_t)w * (uint32_t)h * 2U;

    if(SPI0_BUS_OK != lcd_begin()) {
        return;
    }
    set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    /* 整窗一次推完：避免每行重建 DMA（原先每行数 ms 开销） */
    lcd_cs_low();
    lcd_dc_data();
    (void)bsp_spi0_bus_write((const uint8_t *)px, nbytes);
    lcd_cs_high();
    lcd_end();
}
