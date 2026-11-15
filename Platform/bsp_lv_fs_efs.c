/*!
    \file    bsp_lv_fs_efs.c
    \brief   LVGL 文件系统驱动：把 EFS（外置 SPI Flash）挂到盘符 E:
*/

#include "bsp_lv_fs_efs.h"
#include "bsp_efs.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    efs_file_info_t info;
    uint32_t        pos;
    uint8_t         used;
} efs_file_t;

#define EFS_FS_MAX_OPEN  8U

static efs_file_t s_files[EFS_FS_MAX_OPEN];
static lv_fs_drv_t s_drv;
static uint8_t s_inited;

static bool efs_fs_ready(lv_fs_drv_t *drv)
{
    (void)drv;
    return (0U != bsp_efs_ready()) ? true : false;
}

static void *efs_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    uint8_t i;
    efs_file_t *f;
    const char *name = path;

    (void)drv;
    if(0U == (mode & LV_FS_MODE_RD)) {
        return NULL;
    }
    if((NULL == name) || ('\0' == name[0])) {
        return NULL;
    }
    /* 允许 "font_h1.bin" 或 "/font_h1.bin" */
    if('/' == name[0]) {
        name++;
    }

    for(i = 0U; i < EFS_FS_MAX_OPEN; i++) {
        if(0U == s_files[i].used) {
            break;
        }
    }
    if(i >= EFS_FS_MAX_OPEN) {
        return NULL;
    }

    f = &s_files[i];
    if(EFS_OK != bsp_efs_find(name, &f->info)) {
        return NULL;
    }
    f->pos = 0U;
    f->used = 1U;
    return f;
}

static lv_fs_res_t efs_fs_close(lv_fs_drv_t *drv, void *file_p)
{
    efs_file_t *f = (efs_file_t *)file_p;

    (void)drv;
    if(NULL == f) {
        return LV_FS_RES_INV_PARAM;
    }
    f->used = 0U;
    f->pos = 0U;
    return LV_FS_RES_OK;
}

static lv_fs_res_t efs_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    efs_file_t *f = (efs_file_t *)file_p;
    uint32_t remain;
    uint32_t n;

    (void)drv;
    if((NULL == f) || (NULL == buf) || (NULL == br)) {
        return LV_FS_RES_INV_PARAM;
    }
    *br = 0U;
    if(f->pos >= f->info.size) {
        return LV_FS_RES_OK;
    }
    remain = f->info.size - f->pos;
    n = (btr < remain) ? btr : remain;
    if(EFS_OK != bsp_efs_read(f->info.name, f->pos, (uint8_t *)buf, n)) {
        return LV_FS_RES_HW_ERR;
    }
    f->pos += n;
    *br = n;
    return LV_FS_RES_OK;
}

static lv_fs_res_t efs_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    efs_file_t *f = (efs_file_t *)file_p;
    uint32_t np;

    (void)drv;
    if(NULL == f) {
        return LV_FS_RES_INV_PARAM;
    }
    if(LV_FS_SEEK_SET == whence) {
        np = pos;
    } else if(LV_FS_SEEK_CUR == whence) {
        np = f->pos + pos;
    } else if(LV_FS_SEEK_END == whence) {
        np = f->info.size + pos;
    } else {
        return LV_FS_RES_INV_PARAM;
    }
    if(np > f->info.size) {
        return LV_FS_RES_INV_PARAM;
    }
    f->pos = np;
    return LV_FS_RES_OK;
}

static lv_fs_res_t efs_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    efs_file_t *f = (efs_file_t *)file_p;

    (void)drv;
    if((NULL == f) || (NULL == pos_p)) {
        return LV_FS_RES_INV_PARAM;
    }
    *pos_p = f->pos;
    return LV_FS_RES_OK;
}

uint8_t bsp_lv_fs_efs_init(void)
{
    if(0U != s_inited) {
        return 0U;
    }
    memset(s_files, 0, sizeof(s_files));
    lv_fs_drv_init(&s_drv);
    s_drv.letter = BSP_LV_FS_EFS_LETTER;
    s_drv.cache_size = 0U;
    s_drv.ready_cb = efs_fs_ready;
    s_drv.open_cb = efs_fs_open;
    s_drv.close_cb = efs_fs_close;
    s_drv.read_cb = efs_fs_read;
    s_drv.seek_cb = efs_fs_seek;
    s_drv.tell_cb = efs_fs_tell;
    lv_fs_drv_register(&s_drv);
    s_inited = 1U;
    printf("[LVFS] EFS drive %c: ready\r\n", BSP_LV_FS_EFS_LETTER);
    return 0U;
}
