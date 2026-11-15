#include "bsp_ntc.h"
#include "board.h"
#include "systick.h"
#include <math.h>

#define NTC_ADC_MAX     4095U
#define NTC_T25_K       298.15f

static uint16_t s_raw = 0U;
static uint32_t s_res_ohm = 0U;
static int16_t  s_temp_x10 = 0;

static uint16_t ntc_read_adc(void)
{
    /* 连续转换模式下直接读最新结果 */
    return (uint16_t)adc_routine_data_read(BOARD_NTC_ADC);
}

static void ntc_calc(uint16_t raw)
{
    float r;
    float t_k;
    float ln_rr;

    s_raw = raw;

    if(raw >= (NTC_ADC_MAX - 5U)) {
        /* 接近满量程：对地短路或开路异常 */
        s_res_ohm = 0U;
        s_temp_x10 = 9999;
        return;
    }
    if(raw <= 5U) {
        s_res_ohm = 0xFFFFFFFFU;
        s_temp_x10 = -9999;
        return;
    }

    /* Rntc = Rpull * adc / (4095 - adc) */
    r = ((float)BOARD_NTC_PULLUP_OHM * (float)raw) / (float)(NTC_ADC_MAX - raw);
    s_res_ohm = (uint32_t)(r + 0.5f);

    /* Beta 公式：1/T = 1/T25 + ln(R/R25)/B */
    ln_rr = logf(r / (float)BOARD_NTC_R25_OHM);
    t_k = 1.0f / (1.0f / NTC_T25_K + ln_rr / (float)BOARD_NTC_BETA);
    s_temp_x10 = (int16_t)((t_k - 273.15f) * 10.0f + ((t_k >= 273.15f) ? 0.5f : -0.5f));
}

void bsp_ntc_init(void)
{
    rcu_periph_clock_enable(BOARD_NTC_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_NTC_ADC_CLK);
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);

    gpio_mode_set(BOARD_NTC_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, BOARD_NTC_GPIO_PIN);

    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(BOARD_NTC_ADC, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(BOARD_NTC_ADC, ADC_SCAN_MODE, DISABLE);
    adc_data_alignment_config(BOARD_NTC_ADC, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(BOARD_NTC_ADC, ADC_ROUTINE_CHANNEL, 1);
    adc_routine_channel_config(BOARD_NTC_ADC, 0, BOARD_NTC_ADC_CHANNEL, BOARD_NTC_ADC_SAMPLETIME);

    adc_external_trigger_source_config(BOARD_NTC_ADC, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T0_CH0);
    adc_external_trigger_config(BOARD_NTC_ADC, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    adc_enable(BOARD_NTC_ADC);
    delay_1ms(1);
    adc_calibration_enable(BOARD_NTC_ADC);
    adc_software_trigger_enable(BOARD_NTC_ADC, ADC_ROUTINE_CHANNEL);

    s_raw = 0U;
    s_res_ohm = 0U;
    s_temp_x10 = 0;
}

void bsp_ntc_task(void)
{
    ntc_calc(ntc_read_adc());
}

uint16_t bsp_ntc_get_raw(void)
{
    return s_raw;
}

uint32_t bsp_ntc_get_resistance(void)
{
    return s_res_ohm;
}

int16_t bsp_ntc_get_temp_x10(void)
{
    return s_temp_x10;
}
