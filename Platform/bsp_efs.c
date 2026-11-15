/*!
    \file    bsp_efs.c
    \brief   外置 Flash 文件管理：映射表存外置 Flash 首扇区
*/

#include "bsp_efs.h"
#include "bsp_spi_flash.h"
#include <stdio.h>
#include <string.h>

#define EFS_MAGIC           0x45465332UL /* 'EFS2' */
#define EFS_META_ADDR       0x000000UL
#define EFS_DATA_BASE       SPI_FLASH_SECTOR_SIZE
#define EFS_ALIGN(x)        (((x) + (SPI_FLASH_SECTOR_SIZE - 1U)) & ~(SPI_FLASH_SECTOR_SIZE - 1U))

typedef struct {
    char     name[EFS_NAME_MAX];
    uint32_t start;
    uint32_t end;
    uint32_t size;
    uint8_t  used;
    uint8_t  rsv[3];
} efs_entry_t;

typedef struct {
    uint32_t   magic;
    uint16_t   version;
    uint16_t   count;
    uint32_t   data_base;
    uint32_t   next_free;
    uint32_t   seq;
    efs_entry_t files[EFS_MAX_FILES];
    uint32_t   crc; /* 简单累加校验：magic..files */
} efs_meta_t;

static efs_meta_t s_meta;
static uint8_t    s_ready = 0U;
static uint8_t    s_writing = 0U;
static uint8_t    s_wr_auto = 0U;      /* 1=未知最终大小，按需擦除 */
static uint32_t   s_wr_start = 0U;
static uint32_t   s_wr_max = 0U;       /* 允许写入的最大长度 */
static uint32_t   s_wr_erased_to = 0U; /* 已擦除到的绝对地址（不含） */

static uint32_t efs_crc(const efs_meta_t *m)
{
    const uint8_t *p = (const uint8_t *)m;
    uint32_t n = (uint32_t)((uint8_t *)&m->crc - (uint8_t *)m);
    uint32_t c = 0U;
    uint32_t i;

    for(i = 0U; i < n; i++) {
        c = (c + p[i]) + ((c << 1) | (c >> 31));
    }
    return c;
}

static void efs_meta_reset(efs_meta_t *m)
{
    memset(m, 0, sizeof(*m));
    m->magic = EFS_MAGIC;
    m->version = 1U;
    m->count = 0U;
    m->data_base = EFS_DATA_BASE;
    m->next_free = EFS_DATA_BASE;
    m->seq = 1U;
    m->crc = efs_crc(m);
}

static uint8_t efs_meta_save(void)
{
    s_meta.crc = efs_crc(&s_meta);
    if(SPI_FLASH_OK != bsp_spi_flash_sector_erase(EFS_META_ADDR)) {
        return EFS_ERROR;
    }
    if(SPI_FLASH_OK != bsp_spi_flash_write(EFS_META_ADDR, (const uint8_t *)&s_meta, sizeof(s_meta))) {
        return EFS_ERROR;
    }
    return EFS_OK;
}

static uint8_t efs_meta_load(void)
{
    efs_meta_t tmp;

    if(SPI_FLASH_OK != bsp_spi_flash_read(EFS_META_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        return EFS_ERROR;
    }
    if((EFS_MAGIC != tmp.magic) || (tmp.crc != efs_crc(&tmp)) ||
       (tmp.data_base != EFS_DATA_BASE) || (tmp.next_free < EFS_DATA_BASE) ||
       (tmp.next_free > SPI_FLASH_SIZE) || (tmp.count > EFS_MAX_FILES)) {
        return EFS_ERROR;
    }
    s_meta = tmp;
    return EFS_OK;
}

static uint8_t efs_erase_range(uint32_t start, uint32_t size)
{
    uint32_t a;
    uint32_t end;

    if(0U == size) {
        return EFS_OK;
    }
    end = start + size;
    a = start & ~(SPI_FLASH_SECTOR_SIZE - 1U);
    while(a < end) {
        if(SPI_FLASH_OK != bsp_spi_flash_sector_erase(a)) {
            return EFS_ERROR;
        }
        a += SPI_FLASH_SECTOR_SIZE;
    }
    return EFS_OK;
}

uint8_t bsp_efs_init(void)
{
    s_ready = 0U;
    s_writing = 0U;

    if(SPI_FLASH_OK != bsp_spi_flash_init()) {
        printf("[EFS] spi flash init fail\r\n");
        return EFS_ERROR;
    }

    if(EFS_OK != efs_meta_load()) {
        printf("[EFS] no valid map, format...\r\n");
        efs_meta_reset(&s_meta);
        if(EFS_OK != efs_meta_save()) {
            printf("[EFS] format fail\r\n");
            return EFS_ERROR;
        }
    }

    s_ready = 1U;
    printf("[EFS] OK files=%u next=0x%08lX free=%lu\r\n",
           (unsigned)s_meta.count,
           (unsigned long)s_meta.next_free,
           (unsigned long)bsp_efs_free_bytes());
    return EFS_OK;
}

uint8_t bsp_efs_ready(void)
{
    return s_ready;
}

uint8_t bsp_efs_clear_all(void)
{
    if(0U == s_ready) {
        return EFS_ERROR;
    }
    s_writing = 0U;
    /* 只重建映射表（快）。旧数据区会被下次写入覆盖扇区。 */
    printf("[EFS] reset file map (soft clear)...\r\n");
    efs_meta_reset(&s_meta);
    if(EFS_OK != efs_meta_save()) {
        return EFS_ERROR;
    }
    printf("[EFS] cleared\r\n");
    return EFS_OK;
}

void bsp_efs_list(void)
{
    uint8_t i;
    uint8_t n = 0U;

    if(0U == s_ready) {
        printf("[EFS] not ready\r\n");
        return;
    }

    printf("[EFS] ---- file map (ext flash) ----\r\n");
    printf(" next_free=0x%08lX  free=%lu bytes\r\n",
           (unsigned long)s_meta.next_free,
           (unsigned long)bsp_efs_free_bytes());
    for(i = 0U; i < EFS_MAX_FILES; i++) {
        if(0U == s_meta.files[i].used) {
            continue;
        }
        n++;
        printf(" [%u] %-28s 0x%08lX..0x%08lX  %lu B\r\n",
               (unsigned)n,
               s_meta.files[i].name,
               (unsigned long)s_meta.files[i].start,
               (unsigned long)s_meta.files[i].end,
               (unsigned long)s_meta.files[i].size);
    }
    if(0U == n) {
        printf(" (empty)\r\n");
    }
}

uint8_t bsp_efs_count(void)
{
    return (0U != s_ready) ? (uint8_t)s_meta.count : 0U;
}

uint32_t bsp_efs_next_free(void)
{
    return (0U != s_ready) ? s_meta.next_free : 0U;
}

uint32_t bsp_efs_free_bytes(void)
{
    if((0U == s_ready) || (s_meta.next_free >= SPI_FLASH_SIZE)) {
        return 0U;
    }
    return SPI_FLASH_SIZE - s_meta.next_free;
}

uint8_t bsp_efs_get(uint8_t index, efs_file_info_t *out)
{
    uint8_t i;
    uint8_t n = 0U;

    if((NULL == out) || (0U == s_ready)) {
        return EFS_BAD_PARAM;
    }
    for(i = 0U; i < EFS_MAX_FILES; i++) {
        if(0U == s_meta.files[i].used) {
            continue;
        }
        if(n == index) {
            (void)strncpy(out->name, s_meta.files[i].name, EFS_NAME_MAX - 1U);
            out->name[EFS_NAME_MAX - 1U] = '\0';
            out->start = s_meta.files[i].start;
            out->end = s_meta.files[i].end;
            out->size = s_meta.files[i].size;
            return EFS_OK;
        }
        n++;
    }
    return EFS_NOT_FOUND;
}

uint8_t bsp_efs_find(const char *name, efs_file_info_t *out)
{
    uint8_t i;

    if((NULL == name) || (NULL == out) || (0U == s_ready)) {
        return EFS_BAD_PARAM;
    }
    for(i = 0U; i < EFS_MAX_FILES; i++) {
        if((0U != s_meta.files[i].used) &&
           (0 == strncmp(s_meta.files[i].name, name, EFS_NAME_MAX))) {
            (void)strncpy(out->name, s_meta.files[i].name, EFS_NAME_MAX - 1U);
            out->name[EFS_NAME_MAX - 1U] = '\0';
            out->start = s_meta.files[i].start;
            out->end = s_meta.files[i].end;
            out->size = s_meta.files[i].size;
            return EFS_OK;
        }
    }
    return EFS_NOT_FOUND;
}

/* 同名则删掉旧表项（空间不回收，简化；重写走 next_free 追加） */
static void efs_remove_name(const char *name)
{
    uint8_t i;

    for(i = 0U; i < EFS_MAX_FILES; i++) {
        if((0U != s_meta.files[i].used) &&
           (0 == strncmp(s_meta.files[i].name, name, EFS_NAME_MAX))) {
            memset(&s_meta.files[i], 0, sizeof(s_meta.files[i]));
            if(s_meta.count > 0U) {
                s_meta.count--;
            }
            printf("[EFS] overwrite '%s'\r\n", name);
            return;
        }
    }
}

static uint8_t efs_prepare_write_slot(const char *name, uint32_t *out_start, uint32_t *out_max)
{
    uint32_t start;
    uint32_t maxb;

    if(0U == s_ready) {
        return EFS_NOT_READY;
    }
    if((NULL == name) || ('\0' == name[0]) || (NULL == out_start)) {
        return EFS_BAD_PARAM;
    }
    if(0U != s_writing) {
        return EFS_BUSY;
    }

    /* 允许同名覆盖（删表项后追加写） */
    efs_remove_name(name);

    if(s_meta.count >= EFS_MAX_FILES) {
        return EFS_FULL;
    }

    start = EFS_ALIGN(s_meta.next_free);
    if(start >= SPI_FLASH_SIZE) {
        return EFS_FULL;
    }
    maxb = SPI_FLASH_SIZE - start;
    if(0U == maxb) {
        return EFS_FULL;
    }

    *out_start = start;
    if(NULL != out_max) {
        *out_max = maxb;
    }
    return EFS_OK;
}

uint8_t bsp_efs_begin_write_auto(const char *name, uint32_t *out_start, uint32_t *out_max)
{
    uint32_t start;
    uint32_t maxb;
    uint8_t ret;

    ret = efs_prepare_write_slot(name, &start, &maxb);
    if(EFS_OK != ret) {
        return ret;
    }

    s_writing = 1U;
    s_wr_auto = 1U;
    s_wr_start = start;
    s_wr_max = maxb;
    s_wr_erased_to = start; /* 按需擦除 */
    *out_start = start;
    if(NULL != out_max) {
        *out_max = maxb;
    }
    printf("[EFS] begin_auto '%s' start=0x%08lX max=%lu\r\n",
           name, (unsigned long)start, (unsigned long)maxb);
    return EFS_OK;
}

uint8_t bsp_efs_begin_write(const char *name, uint32_t size, uint32_t *out_start)
{
    uint32_t start;
    uint32_t maxb;
    uint32_t need;
    uint8_t ret;

    /* size==0：自动测长模式 */
    if(0U == size) {
        return bsp_efs_begin_write_auto(name, out_start, NULL);
    }

    ret = efs_prepare_write_slot(name, &start, &maxb);
    if(EFS_OK != ret) {
        return ret;
    }
    if(size > maxb) {
        return EFS_FULL;
    }

    need = EFS_ALIGN(size);
    if(EFS_OK != efs_erase_range(start, need)) {
        return EFS_ERROR;
    }

    s_writing = 1U;
    s_wr_auto = 0U;
    s_wr_start = start;
    s_wr_max = size;
    s_wr_erased_to = start + need;
    *out_start = start;
    printf("[EFS] begin '%s' start=0x%08lX size=%lu\r\n",
           name, (unsigned long)start, (unsigned long)size);
    return EFS_OK;
}

uint8_t bsp_efs_write_chunk(uint32_t file_start, uint32_t offset,
                            const uint8_t *data, uint32_t len)
{
    uint32_t abs_end;

    if((0U == s_writing) || (NULL == data) || (0U == len)) {
        return EFS_BAD_PARAM;
    }
    if(file_start != s_wr_start) {
        return EFS_ERROR;
    }
    if((offset + len) > s_wr_max) {
        return EFS_FULL;
    }

    abs_end = file_start + offset + len;
    if(0U != s_wr_auto) {
        while(s_wr_erased_to < abs_end) {
            if(SPI_FLASH_OK != bsp_spi_flash_sector_erase(s_wr_erased_to)) {
                printf("[EFS] sector erase fail @0x%08lX\r\n",
                       (unsigned long)s_wr_erased_to);
                return EFS_ERROR;
            }
            s_wr_erased_to += SPI_FLASH_SECTOR_SIZE;
        }
    }

    if(SPI_FLASH_OK != bsp_spi_flash_write(file_start + offset, data, len)) {
        printf("[EFS] write fail @0x%08lX len=%lu\r\n",
               (unsigned long)(file_start + offset), (unsigned long)len);
        return EFS_ERROR;
    }
    return EFS_OK;
}

uint8_t bsp_efs_commit_write(const char *name, uint32_t file_start, uint32_t size)
{
    uint8_t i;
    uint8_t slot = 0xFFU;

    if((0U == s_writing) || (NULL == name) || (file_start != s_wr_start)) {
        return EFS_BAD_PARAM;
    }
    if((0U == size) || (size > s_wr_max)) {
        return EFS_BAD_PARAM;
    }
    /* 固定大小模式：必须收满；自动模式：允许任意 size<=max */
    if((0U == s_wr_auto) && (size != s_wr_max)) {
        return EFS_BAD_PARAM;
    }

    for(i = 0U; i < EFS_MAX_FILES; i++) {
        if(0U == s_meta.files[i].used) {
            slot = i;
            break;
        }
    }
    if(0xFFU == slot) {
        s_writing = 0U;
        s_wr_auto = 0U;
        return EFS_FULL;
    }

    memset(&s_meta.files[slot], 0, sizeof(s_meta.files[slot]));
    (void)strncpy(s_meta.files[slot].name, name, EFS_NAME_MAX - 1U);
    s_meta.files[slot].start = file_start;
    s_meta.files[slot].end = file_start + size;
    s_meta.files[slot].size = size;
    s_meta.files[slot].used = 1U;
    s_meta.count++;
    s_meta.next_free = EFS_ALIGN(file_start + size);
    s_meta.seq++;
    s_writing = 0U;
    s_wr_auto = 0U;

    if(EFS_OK != efs_meta_save()) {
        return EFS_ERROR;
    }
    printf("[EFS] commit OK '%s' 0x%08lX..0x%08lX size=%lu\r\n",
           name,
           (unsigned long)file_start,
           (unsigned long)(file_start + size),
           (unsigned long)size);
    return EFS_OK;
}

void bsp_efs_abort_write(void)
{
    s_writing = 0U;
    s_wr_auto = 0U;
    s_wr_start = 0U;
    s_wr_max = 0U;
    s_wr_erased_to = 0U;
}

uint8_t bsp_efs_read(const char *name, uint32_t offset, uint8_t *buf, uint32_t len)
{
    efs_file_info_t info;

    if(EFS_OK != bsp_efs_find(name, &info)) {
        return EFS_NOT_FOUND;
    }
    if((offset + len) > info.size) {
        return EFS_BAD_PARAM;
    }
    if(SPI_FLASH_OK != bsp_spi_flash_read(info.start + offset, buf, len)) {
        return EFS_ERROR;
    }
    return EFS_OK;
}
