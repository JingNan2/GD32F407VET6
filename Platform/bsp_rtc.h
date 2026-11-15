#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>

#define BSP_RTC_OK     0U
#define BSP_RTC_ERROR  1U

typedef struct {
    uint16_t year;   /* 2000..2099 */
    uint8_t  month;  /* 1..12 */
    uint8_t  day;    /* 1..31 */
    uint8_t  hour;   /* 0..23 */
    uint8_t  minute; /* 0..59 */
    uint8_t  second; /* 0..59 */
    uint8_t  wday;   /* 1=Mon .. 7=Sun */
} bsp_rtc_time_t;

uint8_t bsp_rtc_init(void);
uint8_t bsp_rtc_get(bsp_rtc_time_t *t);
uint8_t bsp_rtc_set(const bsp_rtc_time_t *t);
uint8_t bsp_rtc_ready(void);

#endif /* BSP_RTC_H */
