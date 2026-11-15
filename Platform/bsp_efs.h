#ifndef BSP_EFS_H
#define BSP_EFS_H

#include <stdint.h>

/*!
    \brief  外置 SPI Flash 简易文件表（映射表也在外置 Flash）

    布局：
      [0x000000, 0x000FFF]  映射表扇区（4KB）
      [0x001000, 末尾)      文件数据区（追加写入）
*/

#define EFS_OK              0U
#define EFS_ERROR           1U
#define EFS_FULL            2U
#define EFS_NOT_FOUND       3U
#define EFS_BAD_PARAM       4U
#define EFS_NOT_READY       5U
#define EFS_NAME_EXISTS     6U
#define EFS_BUSY            7U

#define EFS_MAX_FILES       16U
#define EFS_NAME_MAX        28U

typedef struct {
    char     name[EFS_NAME_MAX];
    uint32_t start; /* 外置 Flash 起始地址（含） */
    uint32_t end;   /* 外置 Flash 结束地址（不含） */
    uint32_t size;
} efs_file_info_t;

uint8_t  bsp_efs_init(void);
uint8_t  bsp_efs_ready(void);

/* 清空映射表 + 整片擦除外置 Flash，数据区从 DATA_BASE 重开 */
uint8_t  bsp_efs_clear_all(void);

/* 打印文件列表到串口 */
void     bsp_efs_list(void);

uint8_t  bsp_efs_get(uint8_t index, efs_file_info_t *out);
uint8_t  bsp_efs_find(const char *name, efs_file_info_t *out);
uint8_t  bsp_efs_count(void);
uint32_t bsp_efs_next_free(void);
uint32_t bsp_efs_free_bytes(void);

/*!
    追加写入新文件：
    1) begin_auto：只给文件名，按空闲空间流式写入（扇区按需擦除）
    2) write_chunk：按偏移写入
    3) commit：用实际收到的 size 登记映射表
    size==0 的 begin_write 等同 begin_auto
*/
uint8_t  bsp_efs_begin_write(const char *name, uint32_t size, uint32_t *out_start);
uint8_t  bsp_efs_begin_write_auto(const char *name, uint32_t *out_start, uint32_t *out_max);
uint8_t  bsp_efs_write_chunk(uint32_t file_start, uint32_t offset,
                             const uint8_t *data, uint32_t len);
uint8_t  bsp_efs_commit_write(const char *name, uint32_t file_start, uint32_t size);
void     bsp_efs_abort_write(void);

/* 读文件数据（供 UI/资源加载） */
uint8_t  bsp_efs_read(const char *name, uint32_t offset, uint8_t *buf, uint32_t len);

#endif /* BSP_EFS_H */
