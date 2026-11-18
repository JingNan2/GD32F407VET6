/*!
    \file    app_ui_files.c
    \brief   LVGL 文件管理：TF(FatFs 0:) / 外置 Flash(EFS)
*/

#include "app_ui_files.h"
#include "bsp_fatfs.h"
#include "bsp_efs.h"
#include "bsp_sdcard.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define UI_FILE_MAX       48U
#define UI_NAME_MAX       64U   /* LFN/exFAT 文件名 */
#define UI_PATH_MAX       96U

typedef enum {
    UI_VOL_TF = 0,
    UI_VOL_EFS
} ui_vol_t;

typedef struct {
    char     name[UI_NAME_MAX];
    uint32_t size;
    uint8_t  is_dir;
} ui_file_ent_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_lbl_title;
static lv_obj_t *s_lbl_path;
static lv_obj_t *s_lbl_status;
static lv_obj_t *s_list;
static lv_obj_t *s_btn_tf;
static lv_obj_t *s_btn_efs;
static lv_obj_t *s_btn_up;

static ui_vol_t     s_vol = UI_VOL_TF;
static char         s_path[UI_PATH_MAX];
static ui_file_ent_t s_ents[UI_FILE_MAX];
static uint8_t      s_ent_cnt;
static volatile uint8_t s_need_refresh;

static void ui_format_size(char *out, uint32_t out_sz, uint32_t bytes)
{
    if(bytes < 1024U) {
        (void)lv_snprintf(out, out_sz, "%lu B", (unsigned long)bytes);
    } else if(bytes < (1024U * 1024U)) {
        (void)lv_snprintf(out, out_sz, "%lu KB", (unsigned long)((bytes + 512U) / 1024U));
    } else {
        (void)lv_snprintf(out, out_sz, "%lu.%lu MB",
                          (unsigned long)(bytes / (1024U * 1024U)),
                          (unsigned long)((bytes / 1024U) % 1024U) / 102U);
    }
}

static void ui_style_btn(lv_obj_t *btn, uint32_t bg, uint32_t fg)
{
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 6, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(fg), 0);
}

static void on_mbox_ok(lv_event_t *e)
{
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);

    if(NULL != mbox) {
        lv_msgbox_close(mbox);
    }
}

static void ui_show_detail(uint8_t idx)
{
    lv_obj_t *mbox;
    lv_obj_t *btn;
    char size_buf[24];
    char body[128];
    ui_file_ent_t *e;

    if(idx >= s_ent_cnt) {
        return;
    }
    e = &s_ents[idx];
    ui_format_size(size_buf, sizeof(size_buf), e->size);

    if(0U != e->is_dir) {
        (void)lv_snprintf(body, sizeof(body), "Type: folder\nName: %s", e->name);
    } else {
        (void)lv_snprintf(body, sizeof(body), "Type: file\nName: %s\nSize: %s",
                          e->name, size_buf);
    }

    mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "File info");
    lv_msgbox_add_text(mbox, body);
    btn = lv_msgbox_add_footer_button(mbox, "OK");
    lv_obj_add_event_cb(btn, on_mbox_ok, LV_EVENT_CLICKED, mbox);
    (void)lv_msgbox_add_close_button(mbox);
}

static void ui_path_join(const char *name)
{
    size_t n = strlen(s_path);

    if((0 == strcmp(name, "..")) || (n >= (UI_PATH_MAX - 2U))) {
        return;
    }
    if((n > 0U) && ('/' != s_path[n - 1U])) {
        s_path[n++] = '/';
        s_path[n] = '\0';
    }
    (void)strncat(s_path, name, UI_PATH_MAX - n - 1U);
}

static void ui_path_up(void)
{
    char *p;
    size_t n = strlen(s_path);

    if(n <= 3U) { /* "0:" or "0:/" */
        (void)strncpy(s_path, "0:/", UI_PATH_MAX - 1U);
        s_path[UI_PATH_MAX - 1U] = '\0';
        return;
    }
    /* strip trailing slash */
    if('/' == s_path[n - 1U]) {
        s_path[n - 1U] = '\0';
        n--;
    }
    p = strrchr(s_path, '/');
    if(NULL == p) {
        (void)strncpy(s_path, "0:/", UI_PATH_MAX - 1U);
        s_path[UI_PATH_MAX - 1U] = '\0';
        return;
    }
    if(p == s_path + 2) { /* "0:/xxx" → keep "0:/" */
        p[1] = '\0';
    } else {
        *p = '\0';
    }
}

static void ui_scan_tf(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    uint8_t i = 0U;

    s_ent_cnt = 0U;
    if(0U == bsp_fatfs_is_mounted()) {
        printf("[UI] TF scan skip (not mounted)\r\n");
        return;
    }

    fr = f_opendir(&dir, s_path);
    if(FR_OK != fr) {
        printf("[UI] opendir '%s' fr=%d\r\n", s_path, (int)fr);
        return;
    }

    for(;;) {
        fr = f_readdir(&dir, &fno);
        if((FR_OK != fr) || (0 == fno.fname[0])) {
            break;
        }
        /* 跳过 . / ..；保留其它项（含短文件名） */
        if((0 == strcmp(fno.fname, ".")) || (0 == strcmp(fno.fname, ".."))) {
            continue;
        }
        if(i >= UI_FILE_MAX) {
            break;
        }
        (void)strncpy(s_ents[i].name, fno.fname, UI_NAME_MAX - 1U);
        s_ents[i].name[UI_NAME_MAX - 1U] = '\0';
        s_ents[i].is_dir = (0U != (fno.fattrib & AM_DIR)) ? 1U : 0U;
        s_ents[i].size = (0U != s_ents[i].is_dir) ? 0U : (uint32_t)fno.fsize;
        printf("[UI] TF #%u %s %s %lu\r\n",
               (unsigned)i,
               (0U != s_ents[i].is_dir) ? "<DIR>" : "FILE",
               s_ents[i].name,
               (unsigned long)s_ents[i].size);
        i++;
    }
    (void)f_closedir(&dir);
    s_ent_cnt = i;
    printf("[UI] TF '%s' items=%u\r\n", s_path, (unsigned)s_ent_cnt);
}

static void ui_scan_efs(void)
{
    uint8_t n;
    uint8_t i;
    efs_file_info_t info;

    s_ent_cnt = 0U;
    if(0U == bsp_efs_ready()) {
        return;
    }
    n = bsp_efs_count();
    if(n > UI_FILE_MAX) {
        n = (uint8_t)UI_FILE_MAX;
    }
    for(i = 0U; i < n; i++) {
        if(EFS_OK != bsp_efs_get(i, &info)) {
            break;
        }
        (void)strncpy(s_ents[i].name, info.name, UI_NAME_MAX - 1U);
        s_ents[i].name[UI_NAME_MAX - 1U] = '\0';
        s_ents[i].is_dir = 0U;
        s_ents[i].size = info.size;
        s_ent_cnt++;
    }
}

static void ui_update_header(void)
{
    char st[72];
    int fr;

    if(UI_VOL_TF == s_vol) {
        lv_label_set_text(s_lbl_title, "TF Card");
        lv_label_set_text(s_lbl_path, s_path);
        if(0U == bsp_sd_is_present()) {
            lv_label_set_text(s_lbl_status, "No TF card");
        } else if(0U == bsp_sd_is_ready()) {
            lv_label_set_text(s_lbl_status, "TF present, SDIO not ready");
        } else if(0U == bsp_fatfs_is_mounted()) {
            fr = bsp_fatfs_last_mount_fr();
            if(13 == fr) {
                lv_label_set_text(s_lbl_status, "Mount fail: no FAT/exFAT");
            } else if(fr < 0) {
                lv_label_set_text(s_lbl_status, "TF ready, not mounted");
            } else {
                (void)lv_snprintf(st, sizeof(st), "Mount fail fr=%d", fr);
                lv_label_set_text(s_lbl_status, st);
            }
        } else {
            (void)lv_snprintf(st, sizeof(st), "Mounted · %u items", (unsigned)s_ent_cnt);
            lv_label_set_text(s_lbl_status, st);
        }
        lv_obj_set_hidden(s_btn_up, false);
    } else {
        lv_label_set_text(s_lbl_title, "Ext Flash (EFS)");
        lv_label_set_text(s_lbl_path, "E:");
        if(0U == bsp_efs_ready()) {
            lv_label_set_text(s_lbl_status, "EFS not ready");
        } else {
            (void)lv_snprintf(st, sizeof(st), "Free %lu KB · %u files",
                              (unsigned long)(bsp_efs_free_bytes() / 1024U),
                              (unsigned)s_ent_cnt);
            lv_label_set_text(s_lbl_status, st);
        }
        lv_obj_set_hidden(s_btn_up, true);
    }
}

static void ui_on_row(lv_event_t *e)
{
    uint8_t idx;
    ui_file_ent_t *ent;

    if(LV_EVENT_CLICKED != lv_event_get_code(e)) {
        return;
    }
    idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(idx >= s_ent_cnt) {
        return;
    }
    ent = &s_ents[idx];

    if((UI_VOL_TF == s_vol) && (0U != ent->is_dir)) {
        ui_path_join(ent->name);
        app_ui_files_refresh();
        return;
    }
    ui_show_detail(idx);
}

static void ui_rebuild_list(void)
{
    uint8_t i;
    char line[56];
    char size_buf[16];

    lv_obj_clean(s_list);

    if(0U == s_ent_cnt) {
        lv_obj_t *empty = lv_label_create(s_list);
        if((UI_VOL_TF == s_vol) && (0U == bsp_fatfs_is_mounted())) {
            lv_label_set_text(empty, "TF not mounted\nTap Refresh");
        } else {
            lv_label_set_text(empty, "(folder empty)");
        }
        lv_obj_set_style_text_color(empty, lv_color_hex(0x8899AA), 0);
        return;
    }

    for(i = 0U; i < s_ent_cnt; i++) {
        lv_obj_t *btn = lv_button_create(s_list);
        lv_obj_t *lbl;
        ui_style_btn(btn, 0x1A233A, 0xEAEAEA);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 40);
        lv_obj_add_event_cb(btn, ui_on_row, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        if(0U != s_ents[i].is_dir) {
            (void)lv_snprintf(line, sizeof(line), "<DIR>  %s", s_ents[i].name);
        } else {
            ui_format_size(size_buf, sizeof(size_buf), s_ents[i].size);
            (void)lv_snprintf(line, sizeof(line), "%s    %s", s_ents[i].name, size_buf);
        }
        lbl = lv_label_create(btn);
        lv_label_set_text(lbl, line);
        lv_obj_center(lbl);
    }
}

static void ui_set_vol_btn_style(void)
{
    if(UI_VOL_TF == s_vol) {
        ui_style_btn(s_btn_tf, 0x4CC9F0, 0x0B1020);
        ui_style_btn(s_btn_efs, 0x1A233A, 0xADB5BD);
    } else {
        ui_style_btn(s_btn_tf, 0x1A233A, 0xADB5BD);
        ui_style_btn(s_btn_efs, 0x4CC9F0, 0x0B1020);
    }
}

void app_ui_files_refresh(void)
{
    if(NULL == s_list) {
        return;
    }
    if(UI_VOL_TF == s_vol) {
        ui_scan_tf();
    } else {
        ui_scan_efs();
    }
    ui_update_header();
    ui_rebuild_list();
    ui_set_vol_btn_style();
    s_need_refresh = 0U;
}

void app_ui_files_request_refresh(void)
{
    s_need_refresh = 1U;
}

void app_ui_files_poll(void)
{
    if(0U != s_need_refresh) {
        app_ui_files_refresh();
    }
}

static void ui_tf_prepare(void)
{
    (void)strncpy(s_path, "0:/", UI_PATH_MAX - 1U);
    s_path[UI_PATH_MAX - 1U] = '\0';

    if(0U == bsp_sd_is_present()) {
        bsp_fatfs_umount();
        return;
    }
    if(0U == bsp_sd_is_ready()) {
        if(BSP_SD_OK != bsp_sd_init()) {
            printf("[UI] SD init fail\r\n");
            return;
        }
    }
    /* 电脑拷完文件再插回：必须 remount，否则可能仍是旧缓存/空目录 */
    (void)bsp_fatfs_remount();
}

static void on_refresh(lv_event_t *e)
{
    (void)e;
    if(UI_VOL_TF == s_vol) {
        ui_tf_prepare();
    }
    app_ui_files_refresh();
}

static void on_vol_tf(lv_event_t *e)
{
    (void)e;
    s_vol = UI_VOL_TF;
    ui_tf_prepare();
    app_ui_files_refresh();
}

static void on_vol_efs(lv_event_t *e)
{
    (void)e;
    s_vol = UI_VOL_EFS;
    app_ui_files_refresh();
}

static void on_up(lv_event_t *e)
{
    (void)e;
    if(UI_VOL_TF == s_vol) {
        ui_path_up();
        app_ui_files_refresh();
    }
}

void app_ui_files_create(lv_obj_t *scr)
{
    lv_obj_t *hdr;
    lv_obj_t *bar;
    lv_obj_t *btn_ref;
    lv_obj_t *lbl;

    s_scr = scr;
    (void)strncpy(s_path, "0:/", UI_PATH_MAX - 1U);
    s_path[UI_PATH_MAX - 1U] = '\0';
    s_ent_cnt = 0U;
    /* TF 已挂载优先看 TF；否则看 EFS（串口上传文件在这） */
    if(0U != bsp_fatfs_is_mounted()) {
        s_vol = UI_VOL_TF;
    } else {
        s_vol = UI_VOL_EFS;
    }

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B1020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    /* Header */
    hdr = lv_obj_create(scr);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, 240, 72);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x121A2B), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(hdr, 8, 0);

    s_lbl_title = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(0xEAEAEA), 0);
    lv_label_set_text(s_lbl_title, "TF Card");
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    btn_ref = lv_button_create(hdr);
    ui_style_btn(btn_ref, 0x4CC9F0, 0x0B1020);
    lv_obj_set_size(btn_ref, 56, 28);
    lv_obj_align(btn_ref, LV_ALIGN_TOP_RIGHT, 0, -2);
    lbl = lv_label_create(btn_ref);
    lv_label_set_text(lbl, "Refresh");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn_ref, on_refresh, LV_EVENT_CLICKED, NULL);

    s_lbl_path = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_path, lv_color_hex(0x4CC9F0), 0);
    lv_label_set_text(s_lbl_path, "0:/");
    lv_obj_align(s_lbl_path, LV_ALIGN_TOP_LEFT, 0, 22);

    s_lbl_status = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x8899AA), 0);
    lv_label_set_text(s_lbl_status, "...");
    lv_obj_align(s_lbl_status, LV_ALIGN_TOP_LEFT, 0, 42);

    /* Up button */
    s_btn_up = lv_button_create(scr);
    ui_style_btn(s_btn_up, 0x1A233A, 0xEAEAEA);
    lv_obj_set_size(s_btn_up, 64, 28);
    lv_obj_set_pos(s_btn_up, 8, 78);
    lbl = lv_label_create(s_btn_up);
    lv_label_set_text(lbl, ".. Up");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(s_btn_up, on_up, LV_EVENT_CLICKED, NULL);

    /* Scroll list */
    s_list = lv_obj_create(scr);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, 224, 140);
    lv_obj_set_pos(s_list, 8, 112);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x0B1020), 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollable(s_list, true);

    /* Bottom volume bar */
    bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 240, 44);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x121A2B), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_btn_tf = lv_button_create(bar);
    lv_obj_set_size(s_btn_tf, 100, 32);
    lbl = lv_label_create(s_btn_tf);
    lv_label_set_text(lbl, "TF");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(s_btn_tf, on_vol_tf, LV_EVENT_CLICKED, NULL);

    s_btn_efs = lv_button_create(bar);
    lv_obj_set_size(s_btn_efs, 100, 32);
    lbl = lv_label_create(s_btn_efs);
    lv_label_set_text(lbl, "EFS");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(s_btn_efs, on_vol_efs, LV_EVENT_CLICKED, NULL);

    app_ui_files_refresh();
    printf("[UI] file browser ready\r\n");
}
