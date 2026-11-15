#ifndef BSP_NTC_H
#define BSP_NTC_H

#include <stdint.h>

void bsp_ntc_init(void);

/* 100ms 周期任务中调用：刷新采样与温度 */
void bsp_ntc_task(void);

/* ADC 原始值 0~4095 */
uint16_t bsp_ntc_get_raw(void);

/* NTC 电阻，单位 Ω */
uint32_t bsp_ntc_get_resistance(void);

/* 温度，单位 0.1℃（例如 253 = 25.3℃） */
int16_t bsp_ntc_get_temp_x10(void);

#endif /* BSP_NTC_H */
