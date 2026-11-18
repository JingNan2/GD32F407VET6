/*!
    \file    diskio.c
    \brief   FatFs 底层：接到 bsp_sd_read / bsp_sd_write（物理盘 0 = TF）
*/

#include "ff.h"
#include "diskio.h"
#include "bsp_sdcard.h"

#define DEV_TF   0

DSTATUS disk_status(BYTE pdrv)
{
    if(DEV_TF != pdrv) {
        return STA_NOINIT;
    }
    if(0U == bsp_sd_is_present()) {
        return STA_NODISK;
    }
    if(0U == bsp_sd_is_ready()) {
        return STA_NOINIT;
    }
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if(DEV_TF != pdrv) {
        return STA_NOINIT;
    }
    if(0U == bsp_sd_is_present()) {
        return STA_NODISK;
    }
    /* SDIO 已由 app 插卡流程 bsp_sd_init() 完成；此处只检查就绪 */
    if(0U == bsp_sd_is_ready()) {
        if(BSP_SD_OK != bsp_sd_init()) {
            return STA_NOINIT;
        }
    }
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if(DEV_TF != pdrv) {
        return RES_PARERR;
    }
    if((NULL == buff) || (0U == count)) {
        return RES_PARERR;
    }
    if(0U == bsp_sd_is_ready()) {
        return RES_NOTRDY;
    }
    if(BSP_SD_OK != bsp_sd_read(buff, (uint32_t)sector, (uint32_t)count)) {
        return RES_ERROR;
    }
    return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if(DEV_TF != pdrv) {
        return RES_PARERR;
    }
    if((NULL == buff) || (0U == count)) {
        return RES_PARERR;
    }
    if(0U == bsp_sd_is_ready()) {
        return RES_NOTRDY;
    }
    if(BSP_SD_OK != bsp_sd_write((uint8_t *)buff, (uint32_t)sector, (uint32_t)count)) {
        return RES_ERROR;
    }
    return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if(DEV_TF != pdrv) {
        return RES_PARERR;
    }

    switch(cmd) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
        if(NULL == buff) {
            return RES_PARERR;
        }
        if(0U == bsp_sd_is_ready()) {
            return RES_NOTRDY;
        }
        *(DWORD *)buff = (DWORD)bsp_sd_get_sector_count();
        return RES_OK;

    case GET_SECTOR_SIZE:
        if(NULL == buff) {
            return RES_PARERR;
        }
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if(NULL == buff) {
            return RES_PARERR;
        }
        *(DWORD *)buff = 1; /* erase block in unit of sector */
        return RES_OK;

    default:
        return RES_PARERR;
    }
}
