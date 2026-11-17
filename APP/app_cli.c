/*!
    \file    app_cli.c
    \brief   串口命令树：校时 + 外置 Flash 文件管理
*/

#include "app_cli.h"
#include "bsp_usart.h"
#include "bsp_rtc.h"
#include "bsp_efs.h"
#include "bsp_lvgl.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define CLI_LINE_MAX          96U
#define CLI_UPDATE_MS        30000U
#define CLI_IDLE_SUBMIT_MS    100U
#define CLI_WRITE_CHUNK       4096U   /* DMA 批量落盘，减少 Flash 次数 */
#define CLI_WRITE_START_TO_MS 30000U
#define CLI_WRITE_IDLE_END_MS 2000U   /* 主机“发送文件”前后常有停顿 */
#define CLI_WRITE_HARD_TO_MS  180000U

typedef enum {
    CLI_ST_ROOT = 0,
    CLI_ST_UPDATE_TIME,
    CLI_ST_WRITE_DATA
} cli_state_t;

typedef struct cli_node {
    const char *name;
    const char *help;
    void (*on_enter)(void);
} cli_node_t;

static cli_state_t s_st = CLI_ST_ROOT;
static char        s_line[CLI_LINE_MAX];
static uint8_t     s_len = 0U;
static uint32_t    s_wait_start_ms = 0U;
static uint32_t    s_last_rx_ms = 0U;
static uint8_t     s_line_active = 0U;

/* WriteFile 会话（自动测长：静默结束） */
static char     s_wf_name[EFS_NAME_MAX];
static uint32_t s_wf_max = 0U;     /* 固定长度模式用；0=自动 */
static uint32_t s_wf_got = 0U;
static uint32_t s_wf_start = 0U;
static uint8_t  s_wf_auto = 0U;
static uint8_t  s_wf_buf[CLI_WRITE_CHUNK];
static uint16_t s_wf_buf_len = 0U;
static uint32_t s_wf_last_rx_ms = 0U;
static uint32_t s_wf_session_ms = 0U;

static void cli_trim(char *s)
{
    char *p = s;
    char *end;

    while((*p != '\0') && isspace((unsigned char)*p)) {
        p++;
    }
    if(p != s) {
        (void)memmove(s, p, strlen(p) + 1U);
    }
    end = s + strlen(s);
    while((end > s) && isspace((unsigned char)end[-1])) {
        end--;
        *end = '\0';
    }
}

static int cli_str_ieq(const char *a, const char *b)
{
    while((*a != '\0') && (*b != '\0')) {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0') && (*b == '\0');
}

static void cli_enter_update_time(void)
{
    s_st = CLI_ST_UPDATE_TIME;
    s_wait_start_ms = sys_tick_get();
    s_len = 0U;
    s_line[0] = '\0';
    printf("EnterUpdateTime (HH:MM:SS), timeout %lus\r\n",
           (unsigned long)(CLI_UPDATE_MS / 1000U));
}

static uint8_t cli_write_begin(const char *name, uint32_t fixed_size)
{
    uint8_t ret;
    uint32_t maxb = 0U;

    if((NULL == name) || ('\0' == name[0]) || (strlen(name) >= EFS_NAME_MAX)) {
        printf("WriteFile bad name\r\n");
        return 0U;
    }

    bsp_efs_abort_write();
    (void)strncpy(s_wf_name, name, EFS_NAME_MAX - 1U);
    s_wf_name[EFS_NAME_MAX - 1U] = '\0';
    s_wf_got = 0U;
    s_wf_buf_len = 0U;
    s_wf_max = fixed_size;
    s_wf_auto = (0U == fixed_size) ? 1U : 0U;

    if(0U != s_wf_auto) {
        ret = bsp_efs_begin_write_auto(s_wf_name, &s_wf_start, &maxb);
        s_wf_max = maxb;
    } else {
        if(fixed_size > bsp_efs_free_bytes()) {
            printf("No space (free=%lu)\r\n", (unsigned long)bsp_efs_free_bytes());
            return 0U;
        }
        ret = bsp_efs_begin_write(s_wf_name, fixed_size, &s_wf_start);
    }
    if(EFS_OK != ret) {
        printf("begin_write fail code=%u", (unsigned)ret);
        if(EFS_NOT_READY == ret) {
            printf(" (EFS not ready)");
        } else if(EFS_BAD_PARAM == ret) {
            printf(" (bad param)");
        } else if(EFS_FULL == ret) {
            printf(" (full, try /ClearEFlash)");
        } else if(EFS_BUSY == ret) {
            printf(" (busy)");
        } else if(EFS_NAME_EXISTS == ret) {
            printf(" (name exists)");
        }
        printf("\r\n");
        bsp_efs_abort_write();
        return 0U;
    }

    s_st = CLI_ST_WRITE_DATA;
    s_wf_session_ms = sys_tick_get();
    s_wf_last_rx_ms = 0U; /* 尚无数据 */
    s_wait_start_ms = s_wf_session_ms;
    s_len = 0U;
    s_line[0] = '\0';
    s_line_active = 0U;
    bsp_lvgl_pause(); /* 避免与 Flash 抢 SPI0 */
    /* DMA1_CH2 循环收 USART；期间 Flash 自动改轮询，保证不丢字节 */
    bsp_usart_rx_dma_highspeed_enter();

    if(0U != s_wf_auto) {
        printf("SendBinary NOW (DMA RX). Idle %ums => auto size. max=%lu\r\n",
               (unsigned)CLI_WRITE_IDLE_END_MS, (unsigned long)s_wf_max);
    } else {
        printf("SendBinary %lu bytes NOW (DMA RX, no echo)\r\n", (unsigned long)s_wf_max);
    }
    return 1U;
}

static void cli_enter_write_file(void)
{
    printf("Usage: /WriteFile <name>   then send BIN (size auto)\r\n");
    printf("   or: /WriteFile <name> <size>  fixed length\r\n");
}

static void cli_cmd_clear_eflash(void)
{
    printf("ClearEFlash...\r\n");
    if(EFS_OK != bsp_efs_clear_all()) {
        printf("ClearEFlash FAIL\r\n");
    } else {
        printf("ClearEFlash OK\r\n");
    }
}

static void cli_cmd_list_files(void)
{
    bsp_efs_list();
}

static void cli_cmd_help(void);

static const cli_node_t s_root[] = {
    { "/help",       "list commands",                 cli_cmd_help },
    { "/UpdateTime", "calibrate RTC HH:MM:SS",        cli_enter_update_time },
    { "/WriteFile",  "/WriteFile name  then BIN (auto size)", cli_enter_write_file },
    { "/ClearEFlash","erase all ext Flash + map",     cli_cmd_clear_eflash },
    { "/ListFiles",  "list ext Flash file map",       cli_cmd_list_files },
};

static void cli_cmd_help(void)
{
    uint32_t i;

    printf("Available commands:\r\n");
    for(i = 0U; i < (sizeof(s_root) / sizeof(s_root[0])); i++) {
        printf("  %s  - %s\r\n", s_root[i].name, s_root[i].help);
    }
}

static const char *cli_skip_ws(const char *p)
{
    while((NULL != p) && ((*p == ' ') || (*p == '\t'))) {
        p++;
    }
    return p;
}

static uint8_t cli_starts_with_ieq(const char *line, const char *prefix)
{
    while(('\0' != *prefix) && ('\0' != *line)) {
        if(tolower((unsigned char)*line) != tolower((unsigned char)*prefix)) {
            return 0U;
        }
        line++;
        prefix++;
    }
    return ('\0' == *prefix) ? 1U : 0U;
}

static uint8_t cli_try_writefile_oneline(const char *line)
{
    const char *p;
    char name[EFS_NAME_MAX];
    uint32_t fixed = 0U;
    uint32_t i = 0U;
    unsigned long sz;
    char *end = NULL;

    /* 手动解析，避免某些库 sscanf 对可选参数异常 */
    if(0U == cli_starts_with_ieq(line, "/WriteFile")) {
        return 0U;
    }
    p = line + 10; /* strlen("/WriteFile") */

    /* 恰好 "/WriteFile" → 打印用法 */
    p = cli_skip_ws(p);
    if('\0' == *p) {
        cli_enter_write_file();
        return 1U;
    }

    while(('\0' != *p) && (' ' != *p) && ('\t' != *p) && (i < (EFS_NAME_MAX - 1U))) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    if(0U == i) {
        printf("WriteFile need name\r\n");
        return 1U;
    }

    p = cli_skip_ws(p);
    if('\0' != *p) {
        sz = strtoul(p, &end, 10);
        if((NULL == end) || (end == p) || (0UL == sz)) {
            printf("WriteFile bad optional size\r\n");
            return 1U;
        }
        fixed = (uint32_t)sz;
    }

    printf("[CLI] WriteFile name='%s' mode=%s\r\n",
           name, (0U == fixed) ? "auto" : "fixed");
    if(0U == cli_write_begin(name, fixed)) {
        s_st = CLI_ST_ROOT;
    }
    return 1U;
}

static uint8_t cli_line_is_cmd_like(const char *line)
{
    if((NULL == line) || ('\0' == line[0])) {
        return 0U;
    }
    /* 只接受以 / 开头的可打印命令；BIN 误入时静默丢弃 */
    if('/' != line[0]) {
        return 0U;
    }
    while('\0' != *line) {
        if(((unsigned char)*line < 0x20U) || ((unsigned char)*line > 0x7EU)) {
            return 0U;
        }
        line++;
    }
    return 1U;
}

static void cli_dispatch_root(const char *line)
{
    uint32_t i;

    if((NULL == line) || ('\0' == line[0])) {
        return;
    }
    if(0U == cli_line_is_cmd_like(line)) {
        return;
    }

    if(0U != cli_try_writefile_oneline(line)) {
        return;
    }

    for(i = 0U; i < (sizeof(s_root) / sizeof(s_root[0])); i++) {
        if(0 != cli_str_ieq(line, s_root[i].name)) {
            if(NULL != s_root[i].on_enter) {
                s_root[i].on_enter();
            }
            return;
        }
    }
    printf("Unknown: %s  (try /help)\r\n", line);
}

static uint8_t cli_parse_hms(const char *s, uint8_t *h, uint8_t *m, uint8_t *sec)
{
    unsigned hh;
    unsigned mm;
    unsigned ss;
    char extra;
    int n;

    n = sscanf(s, "%u:%u:%u%c", &hh, &mm, &ss, &extra);
    if(3 != n) {
        return 0U;
    }
    if((hh > 23U) || (mm > 59U) || (ss > 59U)) {
        return 0U;
    }
    *h = (uint8_t)hh;
    *m = (uint8_t)mm;
    *sec = (uint8_t)ss;
    return 1U;
}

static void cli_exit_update_time(void)
{
    s_st = CLI_ST_ROOT;
    s_len = 0U;
    s_line[0] = '\0';
    printf("ExitUpdateTime\r\n");
}

static void cli_exit_write_file(const char *msg)
{
    bsp_usart_rx_dma_highspeed_exit();
    bsp_efs_abort_write();
    bsp_lvgl_resume();
    s_st = CLI_ST_ROOT;
    s_len = 0U;
    s_line[0] = '\0';
    s_wf_buf_len = 0U;
    s_line_active = 0U;
    printf("%s\r\n", (NULL != msg) ? msg : "ExitWriteFile");
}

static void cli_handle_update_time_line(const char *line)
{
    uint8_t h;
    uint8_t m;
    uint8_t sec;
    bsp_rtc_time_t t;

    if(0U == cli_parse_hms(line, &h, &m, &sec)) {
        printf("Bad format, need HH:MM:SS, retry\r\n");
        s_wait_start_ms = sys_tick_get();
        return;
    }
    if(BSP_RTC_OK != bsp_rtc_get(&t)) {
        memset(&t, 0, sizeof(t));
        t.year = 2026U;
        t.month = 1U;
        t.day = 1U;
        t.wday = 4U;
    }
    t.hour = h;
    t.minute = m;
    t.second = sec;
    if(BSP_RTC_OK != bsp_rtc_set(&t)) {
        printf("UpdateTime fail (RTC)\r\n");
        s_wait_start_ms = sys_tick_get();
        return;
    }
    printf("UpdateTime OK %02u:%02u:%02u\r\n",
           (unsigned)h, (unsigned)m, (unsigned)sec);
    s_st = CLI_ST_ROOT;
}

static void cli_write_flush_buf(void)
{
    if(0U == s_wf_buf_len) {
        return;
    }
    if(EFS_OK != bsp_efs_write_chunk(s_wf_start, s_wf_got - s_wf_buf_len,
                                     s_wf_buf, (uint32_t)s_wf_buf_len)) {
        cli_exit_write_file("WriteFile FAIL (chunk)");
        return;
    }
    s_wf_buf_len = 0U;
}

static void cli_write_finish(void)
{
    cli_write_flush_buf();
    if(CLI_ST_ROOT == s_st) {
        return;
    }
    if(0U == s_wf_got) {
        cli_exit_write_file("WriteFile FAIL (0 bytes)");
        return;
    }
    if(EFS_OK != bsp_efs_commit_write(s_wf_name, s_wf_start, s_wf_got)) {
        cli_exit_write_file("WriteFile FAIL (commit)");
        return;
    }
    bsp_usart_rx_dma_highspeed_exit();
    printf("WriteFile OK name=%s start=0x%08lX end=0x%08lX size=%lu\r\n",
           s_wf_name,
           (unsigned long)s_wf_start,
           (unsigned long)(s_wf_start + s_wf_got),
           (unsigned long)s_wf_got);
    bsp_lvgl_resume();
    s_st = CLI_ST_ROOT;
    s_len = 0U;
    s_line_active = 0U;
}

/*!
    \brief  DMA 环缓批量取数 → 组包写 EFS（WriteFile 全速路径）
*/
static void cli_write_pump(void)
{
    uint32_t room;
    uint32_t n;
    uint32_t remain;

    while(CLI_ST_WRITE_DATA == s_st) {
        if(s_wf_got >= s_wf_max) {
            if(0U == s_wf_auto) {
                cli_write_finish();
            } else {
                cli_exit_write_file("WriteFile FAIL (overflow)");
            }
            return;
        }

        room = CLI_WRITE_CHUNK - (uint32_t)s_wf_buf_len;
        remain = s_wf_max - s_wf_got;
        if(room > remain) {
            room = remain;
        }
        if(0U == room) {
            cli_write_flush_buf();
            if(CLI_ST_ROOT == s_st) {
                return;
            }
            continue;
        }

        n = bsp_usart_read(&s_wf_buf[s_wf_buf_len], room);
        if(0U == n) {
            break;
        }

        s_wf_buf_len = (uint16_t)(s_wf_buf_len + n);
        s_wf_got += n;
        s_wf_last_rx_ms = sys_tick_get();
        s_last_rx_ms = s_wf_last_rx_ms;

        if(s_wf_buf_len >= CLI_WRITE_CHUNK) {
            cli_write_flush_buf();
            if(CLI_ST_ROOT == s_st) {
                return;
            }
        }

        if((0U == s_wf_auto) && (s_wf_got >= s_wf_max)) {
            cli_write_finish();
            return;
        }
    }
}

static void cli_on_line(char *line)
{
    cli_trim(line);
    if('\0' == line[0]) {
        return;
    }

    if(CLI_ST_UPDATE_TIME == s_st) {
        cli_handle_update_time_line(line);
        return;
    }
    if(CLI_ST_WRITE_DATA == s_st) {
        return;
    }
    cli_dispatch_root(line);
}

static void cli_submit_line(void)
{
    s_line[s_len] = '\0';
    cli_on_line(s_line);
    s_len = 0U;
    s_line[0] = '\0';
    s_line_active = 0U;
}

static void cli_feed_byte(uint8_t ch)
{
    /* WriteFile 走 cli_write_pump 批量路径，不在此逐字节处理 */

    /* ROOT：非文本直接丢弃，避免上位机误发 BIN 刷 Unknown */
    if((CLI_ST_ROOT == s_st) &&
       (ch != '\r') && (ch != '\n') && (ch != '\b') && (ch != 0x7FU) &&
       ((ch < 0x20U) || (ch > 0x7EU))) {
        s_len = 0U;
        s_line[0] = '\0';
        s_line_active = 0U;
        return;
    }

    s_last_rx_ms = sys_tick_get();
    s_line_active = 1U;

    if(('\r' != ch) && ('\n' != ch) && ('\b' != ch) && (0x7FU != ch)) {
        bsp_usart_send_byte(ch);
    }

    if(('\r' == ch) || ('\n' == ch)) {
        bsp_usart_send_string("\r\n");
        cli_submit_line();
        return;
    }

    if(('\b' == ch) || (0x7FU == ch)) {
        if(s_len > 0U) {
            s_len--;
            s_line[s_len] = '\0';
            bsp_usart_send_string("\b \b");
        }
        if(0U == s_len) {
            s_line_active = 0U;
        }
        return;
    }

    if(s_len < (CLI_LINE_MAX - 1U)) {
        s_line[s_len++] = (char)ch;
        s_line[s_len] = '\0';
    }
}

void app_cli_init(void)
{
    s_st = CLI_ST_ROOT;
    s_len = 0U;
    s_line[0] = '\0';
    printf("[CLI] ready, type /help\r\n");
}

void app_cli_task(void)
{
    uint8_t ch;

    bsp_usart_poll_rx();

    if((CLI_ST_UPDATE_TIME == s_st) &&
       (0U != sys_tick_expired(s_wait_start_ms, CLI_UPDATE_MS))) {
        cli_exit_update_time();
    }

    if(CLI_ST_WRITE_DATA == s_st) {
        /* DMA 环 → 4KB 组包写 Flash；先抽干再判空闲结束 */
        cli_write_pump();
        if(CLI_ST_WRITE_DATA == s_st) {
            if(0U != sys_tick_expired(s_wf_session_ms, CLI_WRITE_HARD_TO_MS)) {
                cli_exit_write_file("ExitWriteFile (hard timeout)");
            } else if(0U == s_wf_got) {
                if(0U != sys_tick_expired(s_wf_session_ms, CLI_WRITE_START_TO_MS)) {
                    cli_exit_write_file("ExitWriteFile (no data)");
                }
            } else if((0U != s_wf_auto) && (0U != s_wf_last_rx_ms) &&
                      (0U != sys_tick_expired(s_wf_last_rx_ms, CLI_WRITE_IDLE_END_MS))) {
                cli_write_finish();
            }
        }
        return;
    }

    while(0U != bsp_usart_getc(&ch)) {
        cli_feed_byte(ch);
    }

    if((CLI_ST_ROOT == s_st) || (CLI_ST_UPDATE_TIME == s_st)) {
        if((0U != s_line_active) && (0U != s_len) &&
           (0U != sys_tick_expired(s_last_rx_ms, CLI_IDLE_SUBMIT_MS))) {
            bsp_usart_send_string("\r\n");
            cli_submit_line();
        }
    }
}
