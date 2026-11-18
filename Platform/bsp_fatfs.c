/*!
    \file    bsp_fatfs.c
    \brief   FatFs 挂载 / 卸载 / 列目录（盘符 0: = TF）
*/

#include "bsp_fatfs.h"
#include "bsp_sdcard.h"
#include "ff.h"
#include <stdio.h>

static FATFS s_fs __attribute__((aligned(4)));
static uint8_t s_mounted;
static int s_last_fr = 0;

uint8_t bsp_fatfs_is_mounted(void)
{
    return s_mounted;
}

int bsp_fatfs_last_mount_fr(void)
{
    return s_last_fr;
}

uint8_t bsp_fatfs_mount(void)
{
    FRESULT fr;

    if(0U != s_mounted) {
        s_last_fr = (int)FR_OK;
        return BSP_FATFS_OK;
    }
    if(0U == bsp_sd_is_present()) {
        s_last_fr = -1;
        return BSP_FATFS_NOCARD;
    }
    if(0U == bsp_sd_is_ready()) {
        s_last_fr = -2;
        return BSP_FATFS_ERROR;
    }

    fr = f_mount(&s_fs, "0:", 1);
    s_last_fr = (int)fr;
    if(FR_OK != fr) {
        printf("[FAT] mount fail fr=%d (13=no FAT/exFAT volume)\r\n", (int)fr);
        s_mounted = 0U;
        return BSP_FATFS_ERROR;
    }
    s_mounted = 1U;
    printf("[FAT] mount OK (0:)\r\n");
    return BSP_FATFS_OK;
}

void bsp_fatfs_umount(void)
{
    if(0U != s_mounted) {
        (void)f_mount(NULL, "0:", 0);
        s_mounted = 0U;
        printf("[FAT] umount\r\n");
    }
}

uint8_t bsp_fatfs_remount(void)
{
    bsp_fatfs_umount();
    return bsp_fatfs_mount();
}

void bsp_fatfs_list_root(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    uint32_t n = 0U;

    if(0U == s_mounted) {
        printf("[FAT] not mounted\r\n");
        return;
    }

    fr = f_opendir(&dir, "0:/");
    if(FR_OK != fr) {
        printf("[FAT] opendir fail fr=%d\r\n", (int)fr);
        return;
    }

    printf("[FAT] ---- root ----\r\n");
    for(;;) {
        fr = f_readdir(&dir, &fno);
        if((FR_OK != fr) || (0 == fno.fname[0])) {
            break;
        }
        if(fno.fattrib & AM_DIR) {
            printf("  <DIR>  %s\r\n", fno.fname);
        } else {
            printf("  %8lu  %s\r\n", (unsigned long)fno.fsize, fno.fname);
        }
        n++;
    }
    (void)f_closedir(&dir);
    printf("[FAT] ---- %lu items ----\r\n", (unsigned long)n);
}
