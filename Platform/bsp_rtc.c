/*!
    \file    bsp_rtc.c
    \brief   片内 RTC（LXTAL 32.768kHz → 1Hz 日历）
*/

#include "bsp_rtc.h"
#include "bsp_clock.h"
#include "gd32f4xx.h"
#include <stdio.h>

#define RTC_BKP_MAGIC   0xA55A32C6UL
/* 32768 / ((127+1)*(255+1)) = 1 Hz */
#define RTC_PSC_ASYN    127U
#define RTC_PSC_SYN     255U

static uint8_t s_ready = 0U;

static uint8_t bin2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10U) << 4) | (v % 10U));
}

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((((v >> 4) & 0x0FU) * 10U) + (v & 0x0FU));
}

static void rtc_unlock(void)
{
    RTC_WPK = 0xCAU;
    RTC_WPK = 0x53U;
}

static void rtc_lock(void)
{
    RTC_WPK = 0xFFU;
}

static void fill_rtc_para(rtc_parameter_struct *para, const bsp_rtc_time_t *t)
{
    para->year = bin2bcd((uint8_t)(t->year % 100U));
    para->month = bin2bcd(t->month);
    para->date = bin2bcd(t->day);
    para->day_of_week = t->wday;
    para->hour = bin2bcd(t->hour);
    para->minute = bin2bcd(t->minute);
    para->second = bin2bcd(t->second);
    para->factor_asyn = RTC_PSC_ASYN;
    para->factor_syn = RTC_PSC_SYN;
    para->am_pm = RTC_AM;
    para->display_format = RTC_24HOUR;
}

uint8_t bsp_rtc_init(void)
{
    bsp_rtc_time_t def;
    rtc_parameter_struct para;
    ErrStatus st;

    s_ready = 0U;

    if(SET != bsp_lxtal_ready()) {
        printf("[RTC] LXTAL not ready\r\n");
        return BSP_RTC_ERROR;
    }

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();
    rcu_periph_clock_enable(RCU_RTC);

    if(RTC_BKP_MAGIC == RTC_BKP0) {
        (void)rtc_register_sync_wait();
        s_ready = 1U;
        if(BSP_RTC_OK == bsp_rtc_get(&def)) {
            printf("[RTC] keep %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   (unsigned)def.year, (unsigned)def.month, (unsigned)def.day,
                   (unsigned)def.hour, (unsigned)def.minute, (unsigned)def.second);
        }
        return BSP_RTC_OK;
    }

    /* 首次：默认 2026-01-01 12:00:00 周四 */
    def.year = 2026U;
    def.month = 1U;
    def.day = 1U;
    def.hour = 12U;
    def.minute = 0U;
    def.second = 0U;
    def.wday = RTC_THURSDAY;

    fill_rtc_para(&para, &def);
    st = rtc_init(&para);
    if(SUCCESS != st) {
        printf("[RTC] rtc_init fail\r\n");
        return BSP_RTC_ERROR;
    }

    rtc_unlock();
    RTC_BKP0 = RTC_BKP_MAGIC;
    rtc_lock();

    s_ready = 1U;
    printf("[RTC] init %04u-%02u-%02u %02u:%02u:%02u\r\n",
           (unsigned)def.year, (unsigned)def.month, (unsigned)def.day,
           (unsigned)def.hour, (unsigned)def.minute, (unsigned)def.second);
    return BSP_RTC_OK;
}

uint8_t bsp_rtc_ready(void)
{
    return s_ready;
}

uint8_t bsp_rtc_get(bsp_rtc_time_t *t)
{
    rtc_parameter_struct para;

    if((NULL == t) || (0U == s_ready)) {
        return BSP_RTC_ERROR;
    }

    rtc_current_time_get(&para);
    t->year = (uint16_t)(2000U + bcd2bin(para.year));
    t->month = bcd2bin(para.month);
    t->day = bcd2bin(para.date);
    t->hour = bcd2bin(para.hour);
    t->minute = bcd2bin(para.minute);
    t->second = bcd2bin(para.second);
    t->wday = para.day_of_week;
    return BSP_RTC_OK;
}

uint8_t bsp_rtc_set(const bsp_rtc_time_t *t)
{
    rtc_parameter_struct para;

    if((NULL == t) || (SET != bsp_lxtal_ready())) {
        return BSP_RTC_ERROR;
    }

    fill_rtc_para(&para, t);
    if(SUCCESS != rtc_init(&para)) {
        return BSP_RTC_ERROR;
    }

    rtc_unlock();
    RTC_BKP0 = RTC_BKP_MAGIC;
    rtc_lock();
    s_ready = 1U;
    return BSP_RTC_OK;
}
