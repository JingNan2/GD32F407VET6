#ifndef BSP_SDCARD_H
#define BSP_SDCARD_H

#include <stdint.h>
#include "sdcard.h"

#define BSP_SD_OK       0U
#define BSP_SD_ERROR    1U

/* SDIO 初始化失败阶段（数码管左侧 / LED） */
#define BSP_SD_STAGE_OK         0U
#define BSP_SD_STAGE_POWER      1U  /* sd_init */
#define BSP_SD_STAGE_INFO       2U  /* CID/CSD */
#define BSP_SD_STAGE_SELECT     3U  /* RCA select */
#define BSP_SD_STAGE_LOCKED     4U  /* 卡锁定 */
#define BSP_SD_STAGE_BUS        5U  /* 4bit 总线 */
#define BSP_SD_STAGE_DMA        6U  /* DMA 模式 */

/* TF DET=PD3：低电平=有卡（上拉） */
typedef enum {
    BSP_SD_EVT_NONE = 0,
    BSP_SD_EVT_INSERT,
    BSP_SD_EVT_REMOVE
} bsp_sd_evt_t;

extern uint8_t  g_sd_fail_stage;
extern uint32_t g_sd_fail_code;
extern sd_card_info_struct g_sd_cardinfo;

void     bsp_sd_detect_init(void);
uint8_t  bsp_sd_is_present(void);   /* 防抖后的稳定态：1=有卡 */
uint8_t  bsp_sd_is_ready(void);     /* SDIO 已初始化成功 */
bsp_sd_evt_t bsp_sd_detect_task(void); /* 周期调用，建议 50ms */

uint8_t  bsp_sd_init(void);
void     bsp_sd_deinit(void);       /* 掉电并清 ready（拔卡） */
uint32_t bsp_sd_get_sector_count(void);
uint32_t bsp_sd_get_capacity_kb(void);
uint8_t  bsp_sd_read(uint8_t *pbuf, uint32_t sector, uint32_t cnt);
uint8_t  bsp_sd_write(uint8_t *pbuf, uint32_t sector, uint32_t cnt);

#endif /* BSP_SDCARD_H */
