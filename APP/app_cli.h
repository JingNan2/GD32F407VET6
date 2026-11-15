#ifndef APP_CLI_H
#define APP_CLI_H

/*!
    \brief  串口指令树（可扩展）
    - /help
    - /UpdateTime  → 30s 内输入 HH:MM:SS
*/
void app_cli_init(void);
void app_cli_task(void); /* 建议 20ms 周期 */

#endif /* APP_CLI_H */
