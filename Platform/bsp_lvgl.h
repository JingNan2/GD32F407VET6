#ifndef BSP_LVGL_H
#define BSP_LVGL_H

#include <stdint.h>

uint8_t bsp_lvgl_init(void);
void    bsp_lvgl_task_start(void);
void    bsp_lvgl_pause(void);   /* 写外置 Flash 时暂停刷屏，避免抢 SPI0 */
void    bsp_lvgl_resume(void);

#endif /* BSP_LVGL_H */
