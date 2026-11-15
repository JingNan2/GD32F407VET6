#ifndef BSP_CST816_H
#define BSP_CST816_H

#include <stdint.h>

#define CST816_OK           0U
#define CST816_ERROR        1U

#define CST816_TOUCH_W      240U
#define CST816_TOUCH_H      280U

uint8_t bsp_cst816_init(void);
uint8_t bsp_cst816_read_id(uint8_t *chip_id, uint8_t *fw_ver);
void    bsp_cst816_poll(void);
void    bsp_cst816_scan(void); /* 强制读一次坐标（LVGL 用） */
uint8_t bsp_cst816_is_pressed(void);
void    bsp_cst816_get_xy(uint16_t *x, uint16_t *y);
void    bsp_cst816_exti_isr(void);

#endif /* BSP_CST816_H */
