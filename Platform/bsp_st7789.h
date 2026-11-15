#ifndef BSP_ST7789_H
#define BSP_ST7789_H

#include <stdint.h>

#define ST7789_WIDTH    240U
#define ST7789_HEIGHT   280U
#define ST7789_X_SHIFT  0U
#define ST7789_Y_SHIFT  20U

#define ST7789_WHITE    0xFFFFU
#define ST7789_BLACK    0x0000U
#define ST7789_RED      0xF800U
#define ST7789_GREEN    0x07E0U
#define ST7789_BLUE     0x001FU
#define ST7789_YELLOW   0xFFE0U

void bsp_st7789_init(void);
void bsp_st7789_fill(uint16_t color);
void bsp_st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void bsp_st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void bsp_st7789_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void bsp_st7789_draw_vline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void bsp_st7789_backlight(uint8_t on);
/*! RGB565 块传输（LVGL flush），px 为行优先、每像素 2 字节 */
void bsp_st7789_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px);

#endif /* BSP_ST7789_H */
