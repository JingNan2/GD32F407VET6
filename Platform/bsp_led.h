#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdint.h>

#define LED_NUM     8U

/* idx: 1~8 */
void bsp_led_init(void);

/* PNP 总电源：enable=1 打开供电 */
void bsp_led_power(uint8_t enable);

void bsp_led_on(uint8_t idx);
void bsp_led_off(uint8_t idx);
void bsp_led_toggle(uint8_t idx);

/* bit0~bit7 对应 LED1~LED8，1=亮 */
void bsp_led_write(uint8_t mask);
uint8_t bsp_led_read(void);

void bsp_led_all_off(void);
void bsp_led_all_on(void);

#endif /* BSP_LED_H */
