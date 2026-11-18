#ifndef BSP_LVGL_H
#define BSP_LVGL_H

#include <stdint.h>

uint8_t bsp_lvgl_init(void);
void    bsp_lvgl_task_start(void);
/* WriteFile：UART 占用 DMA1_CH2 期间暂停刷屏；结束后 resume 归还屏幕 */
void    bsp_lvgl_pause(void);
void    bsp_lvgl_resume(void);

#endif /* BSP_LVGL_H */
