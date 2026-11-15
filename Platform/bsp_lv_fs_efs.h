#ifndef BSP_LV_FS_EFS_H
#define BSP_LV_FS_EFS_H

#include <stdint.h>

/* LVGL 盘符：路径形如 "E:font_h1.bin" */
#define BSP_LV_FS_EFS_LETTER  'E'

uint8_t bsp_lv_fs_efs_init(void);

#endif /* BSP_LV_FS_EFS_H */
