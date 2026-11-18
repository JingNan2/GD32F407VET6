#ifndef APP_UI_FILES_H
#define APP_UI_FILES_H

#include <stdint.h>
#include "lvgl.h"

/*! 在当前/指定 screen 上创建文件管理界面（替代表盘） */
void app_ui_files_create(lv_obj_t *scr);

/*! 重新扫描当前卷目录（须在 LVGL 线程上下文调用） */
void app_ui_files_refresh(void);

/*! 其它任务请求刷新（线程安全：仅置位，由 LVGL 线程执行） */
void app_ui_files_request_refresh(void);

/*! LVGL 任务周期调用 */
void app_ui_files_poll(void);

#endif /* APP_UI_FILES_H */
