/*!
    \file    usbd_storage_msd.c
    \brief   USB MSC 存储后端 = TF(SDIO)
             - 枚举前写好扇区数
             - 经对齐 bounce 缓冲再 SDIO DMA（USB 缓冲常非 4 字节对齐）
*/

#include "usbd_msc_mem.h"
#include "msc_scsi.h"
#include "bsp_sdcard.h"
#include "bsp_usb_msc.h"
#include <string.h>

static uint8_t s_toc_data[READ_TOC_CMD_LEN] = {
    0x00U, 0x12U, 0x01U, 0x01U, 0x00U, 0x14U, 0x01U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U
};

/* SDIO DMA 要求字对齐 */
static uint32_t s_bounce32[128];
#define s_bounce ((uint8_t *)s_bounce32)

static const int8_t storage_inquiry_data[] = {
    0x00, 0x80, 0x00, 0x01,
    (USBD_STD_INQUIRY_LENGTH - 5U),
    0x00, 0x00, 0x00,
    'G', 'D', '3', '2', ' ', ' ', ' ', ' ',
    'T', 'F', ' ', 'C', 'a', 'r', 'd', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '1', '.', '0', '0'
};

static int8_t storage_init(uint8_t lun);
static int8_t storage_ready(uint8_t lun);
static int8_t storage_write_protected(uint8_t lun);
static int8_t storage_max_lun_get(void);
static int8_t storage_read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t storage_write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);

usbd_mem_cb usbd_tf_storage_fops = {
    .mem_init      = storage_init,
    .mem_ready     = storage_ready,
    .mem_protected = storage_write_protected,
    .mem_read      = storage_read,
    .mem_write     = storage_write,
    .mem_maxlun    = storage_max_lun_get,
    .mem_toc_data  = s_toc_data,
    .mem_inquiry_data = {(uint8_t *)storage_inquiry_data},
    .mem_block_size = {512U},
    .mem_block_len  = {0U}
};

usbd_mem_cb *usbd_mem_fops = &usbd_tf_storage_fops;

void usbd_storage_set_block_count(uint32_t sectors)
{
    usbd_tf_storage_fops.mem_block_len[0] = sectors;
}

static int8_t storage_init(uint8_t lun)
{
    uint32_t sectors;

    (void)lun;
    if(0U == bsp_usb_msc_sd_ready()) {
        return -1;
    }
    sectors = bsp_sd_get_sector_count();
    if(0U == sectors) {
        return -1;
    }
    usbd_tf_storage_fops.mem_block_len[0] = sectors;
    return 0;
}

static int8_t storage_ready(uint8_t lun)
{
    (void)lun;
    return (0U != bsp_usb_msc_sd_ready()) ? 0 : -1;
}

static int8_t storage_write_protected(uint8_t lun)
{
    (void)lun;
    return 0;
}

static int8_t storage_read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    uint16_t i;

    (void)lun;
    if((NULL == buf) || (0U == blk_len)) {
        return -1;
    }

    /* 逐扇区：对齐 DMA + 降低单次阻塞时间 */
    for(i = 0U; i < blk_len; i++) {
        if(BSP_SD_OK != bsp_sd_read(s_bounce, blk_addr + (uint32_t)i, 1U)) {
            return -1;
        }
        memcpy(buf + ((uint32_t)i * 512U), s_bounce, 512U);
    }
    return 0;
}

static int8_t storage_write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    uint16_t i;

    (void)lun;
    if((NULL == buf) || (0U == blk_len)) {
        return -1;
    }

    for(i = 0U; i < blk_len; i++) {
        memcpy(s_bounce, buf + ((uint32_t)i * 512U), 512U);
        if(BSP_SD_OK != bsp_sd_write(s_bounce, blk_addr + (uint32_t)i, 1U)) {
            return -1;
        }
    }
    return 0;
}

static int8_t storage_max_lun_get(void)
{
    return (MEM_LUN_NUM - 1);
}
