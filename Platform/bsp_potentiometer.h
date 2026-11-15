#ifndef BSP_POTENTIOMETER_H
#define BSP_POTENTIOMETER_H

#include <stdint.h>

/* 电位器 ADC+DMA 硬件初始化（平台层） */
void bsp_potentiometer_init(void);

/* 读取 DMA 最新 ADC 原始值，约 0~4095 */
uint16_t bsp_potentiometer_get_raw(void);

#endif /* BSP_POTENTIOMETER_H */
