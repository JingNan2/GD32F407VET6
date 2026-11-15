/*!
    \file    app_cli.c
    \brief   串口命令树：根节点分发，模式节点带超时输入
*/

#include "app_cli.h"
#include "bsp_usart.h"
#include "bsp_rtc.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define CLI_LINE_MAX       64U
#define CLI_UPDATE_MS     30000U
#define CLI_IDLE_SUBMIT_MS 100U /* 无 CR 时：停顿后自动提交（方便十六进制发送） */

typedef enum {
    CLI_ST_ROOT = 0,
    CLI_ST_UPDATE_TIME
} cli_state_t;

typedef struct cli_node {
    const char *name;          /* 如 "/help" */
    const char *help;          /* /help 展示 */
    void (*on_enter)(void);    /* 进入该节点 */
} cli_node_t;

static cli_state_t s_st = CLI_ST_ROOT;
static char        s_line[CLI_LINE_MAX];
static uint8_t     s_len = 0U;
static uint32_t    s_wait_start_ms = 0U;
static uint32_t    s_last_rx_ms = 0U;
static uint8_t     s_line_active = 0U;

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

static void cli_cmd_help(void);

/* 根节点表：后续新指令往这里加一行即可 */
static const cli_node_t s_root[] = {
    { "/help",       "list commands",                 cli_cmd_help },
    { "/UpdateTime", "enter time calibrate HH:MM:SS", cli_enter_update_time },
};

static void cli_cmd_help(void)
{
    uint32_t i;

    printf("Available commands:\r\n");
    for(i = 0U; i < (sizeof(s_root) / sizeof(s_root[0])); i++) {
        printf("  %s  - %s\r\n", s_root[i].name, s_root[i].help);
    }
}

static void cli_dispatch_root(const char *line)
{
    uint32_t i;

    if((NULL == line) || ('\0' == line[0])) {
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

    if((NULL == s) || (NULL == h) || (NULL == m) || (NULL == sec)) {
        return 0U;
    }

    /* 恰好 3 个数；多读到字符则 n==4 视为非法 */
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

static void cli_exit_update_time(const char *reason)
{
    (void)reason;
    s_st = CLI_ST_ROOT;
    s_len = 0U;
    s_line[0] = '\0';
    printf("ExitUpdateTime\r\n");
}

static void cli_handle_update_time_line(const char *line)
{
    uint8_t h;
    uint8_t m;
    uint8_t sec;
    bsp_rtc_time_t t;

    if(0U == cli_parse_hms(line, &h, &m, &sec)) {
        printf("Bad format, need HH:MM:SS, retry (%lus)\r\n",
               (unsigned long)(CLI_UPDATE_MS / 1000U));
        s_wait_start_ms = sys_tick_get();
        return;
    }

    if(BSP_RTC_OK != bsp_rtc_get(&t)) {
        /* RTC 未就绪也尝试只写时间字段 */
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
    s_len = 0U;
    s_line[0] = '\0';
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
    s_last_rx_ms = sys_tick_get();
    s_line_active = 1U;

    /* 回显 */
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
        cli_exit_update_time("timeout");
    }

    while(0U != bsp_usart_getc(&ch)) {
        cli_feed_byte(ch);
    }

    /* 十六进制工具常不带 0D：静默 CLI_IDLE_SUBMIT_MS 后当一行提交 */
    if((0U != s_line_active) && (0U != s_len) &&
       (0U != sys_tick_expired(s_last_rx_ms, CLI_IDLE_SUBMIT_MS))) {
        bsp_usart_send_string("\r\n");
        cli_submit_line();
    }
}
