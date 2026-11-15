#include "bsp_potentiometer.h"
#include "board.h"
#include "systick.h"

/* DMA 循环写入，调试时可 Watch 此变量 */
static __IO uint16_t s_pot_adc_value = 0;

static void pot_rcu_config(void)
{
    rcu_periph_clock_enable(BOARD_POT_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_POT_ADC_CLK);
    rcu_periph_clock_enable(BOARD_POT_DMA_CLK);
    /* ADC 时钟 = PCLK2/8 = 84M/8 = 10.5MHz */
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
}

static void pot_gpio_config(void)
{
    gpio_mode_set(BOARD_POT_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, BOARD_POT_GPIO_PIN);
}

static void pot_dma_config(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(BOARD_POT_DMA, BOARD_POT_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);

    dma_init_struct.periph_addr         = (uint32_t)(&ADC_RDATA(BOARD_POT_ADC));
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr        = (uint32_t)(&s_pot_adc_value);
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number              = 1;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(BOARD_POT_DMA, BOARD_POT_DMA_CH, &dma_init_struct);

    dma_channel_subperipheral_select(BOARD_POT_DMA, BOARD_POT_DMA_CH, BOARD_POT_DMA_SUBPERI);
    dma_circulation_enable(BOARD_POT_DMA, BOARD_POT_DMA_CH);
    dma_channel_enable(BOARD_POT_DMA, BOARD_POT_DMA_CH);
}

static void pot_adc_config(void)
{
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);

    adc_special_function_config(BOARD_POT_ADC, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(BOARD_POT_ADC, ADC_SCAN_MODE, DISABLE);
    adc_data_alignment_config(BOARD_POT_ADC, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(BOARD_POT_ADC, ADC_ROUTINE_CHANNEL, 1);
    adc_routine_channel_config(BOARD_POT_ADC, 0, BOARD_POT_ADC_CHANNEL, BOARD_POT_ADC_SAMPLETIME);

    adc_external_trigger_source_config(BOARD_POT_ADC, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T0_CH0);
    adc_external_trigger_config(BOARD_POT_ADC, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    adc_dma_request_after_last_enable(BOARD_POT_ADC);
    adc_dma_mode_enable(BOARD_POT_ADC);

    adc_enable(BOARD_POT_ADC);
    delay_1ms(1);
    adc_calibration_enable(BOARD_POT_ADC);

    adc_software_trigger_enable(BOARD_POT_ADC, ADC_ROUTINE_CHANNEL);
}

void bsp_potentiometer_init(void)
{
    pot_rcu_config();
    pot_gpio_config();
    pot_dma_config();
    pot_adc_config();
}

uint16_t bsp_potentiometer_get_raw(void)
{
    return (uint16_t)s_pot_adc_value;
}
