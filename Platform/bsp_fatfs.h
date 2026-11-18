#ifndef BSP_FATFS_H
#define BSP_FATFS_H

#include <stdint.h>

#define BSP_FATFS_OK      0U
#define BSP_FATFS_ERROR   1U
#define BSP_FATFS_NOCARD  2U

/*!
    \brief  TF 卡 FatFs 封装（逻辑盘 "0:"）
    依赖：先 bsp_sd_init() 成功，再 bsp_fatfs_mount()
*/
uint8_t bsp_fatfs_mount(void);
/*! 先 umount 再 mount，插卡拷文件后务必用这个 */
uint8_t bsp_fatfs_remount(void);
void    bsp_fatfs_umount(void);
uint8_t bsp_fatfs_is_mounted(void);
/*! 最近一次 f_mount 返回值（FRESULT，0=OK；13=无有效 FAT） */
int     bsp_fatfs_last_mount_fr(void);

/* 串口打印根目录，便于联调 */
void    bsp_fatfs_list_root(void);

#endif /* BSP_FATFS_H */
