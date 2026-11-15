#ifndef BSP_NIXIE_H
#define BSP_NIXIE_H

#include <stdint.h>

#define NIXIE_DIGIT_NUM     8U

/* 码表索引：0~9 数字；10~19 数字+小数点；20='.' 21='-'；22起字母 */
#define NIXIE_IDX_0         0U
#define NIXIE_IDX_DP0       10U
#define NIXIE_IDX_DOT       20U
#define NIXIE_IDX_MINUS     21U
#define NIXIE_IDX_BLANK     0xFFU

void bsp_nixie_init(void);

/* 扫描步进（由 TIMER2 中断调用，APP 不必再调） */
void bsp_nixie_task(void);

/* 与参考一致：显示码表第 num 项到第 id 位(0~7) —— 单帧，不进扫描缓冲 */
void bsp_nixie_display(uint8_t num, uint8_t id);

/* 扫描缓冲：idx 位显示码表 table_idx；blank 用 NIXIE_IDX_BLANK */
void bsp_nixie_set_digit(uint8_t idx, uint8_t table_idx);

/* 左/右 4 位显示 0~9999 */
void bsp_nixie_show_left(uint16_t num, uint8_t leading_zero);
void bsp_nixie_show_right(uint16_t num, uint8_t leading_zero);

/* 左4位显示温度×10，例如 274 → " 27.4" */
void bsp_nixie_show_temp_x10(int16_t temp_x10);

void bsp_nixie_clear(void);

#endif /* BSP_NIXIE_H */
