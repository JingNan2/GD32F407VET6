/*!
    \file    bsp_sdcard.c
    \brief   TF/SD card BSP — 官方 Examples/SDIO/Read_write 流程
*/

#include "bsp_sdcard.h"
#include "board.h"
#include "systick.h"

uint8_t  g_sd_fail_stage = 0U;
uint32_t g_sd_fail_code = 0U;
sd_card_info_struct g_sd_cardinfo;

static uint8_t s_ready = 0U;

/* DET 防抖：连续 N 次相同才认边沿（配合 50ms 任务 ≈ 200ms） */
#define SD_DET_STABLE_N   4U
static uint8_t s_det_raw_last = 0U;
static uint8_t s_det_stable = 0U;
static uint8_t s_det_cnt = 0U;
static uint8_t s_det_inited = 0U;

static uint8_t sd_det_raw(void)
{
    /* 有卡：DET 被卡座拉低 */
    return (RESET == gpio_input_bit_get(BOARD_TF_DET_PORT, BOARD_TF_DET_PIN)) ? 1U : 0U;
}

void bsp_sd_detect_init(void)
{
    rcu_periph_clock_enable(BOARD_TF_DET_CLK);
    gpio_mode_set(BOARD_TF_DET_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_TF_DET_PIN);
    s_det_stable = sd_det_raw();
    s_det_raw_last = s_det_stable;
    s_det_cnt = 0U;
    s_det_inited = 1U;
}

uint8_t bsp_sd_is_present(void)
{
    if(0U == s_det_inited) {
        bsp_sd_detect_init();
    }
    return s_det_stable;
}

uint8_t bsp_sd_is_ready(void)
{
    return s_ready;
}

bsp_sd_evt_t bsp_sd_detect_task(void)
{
    uint8_t now;

    if(0U == s_det_inited) {
        bsp_sd_detect_init();
    }

    now = sd_det_raw();
    if(now == s_det_stable) {
        s_det_raw_last = now;
        s_det_cnt = 0U;
        return BSP_SD_EVT_NONE;
    }

    if(now == s_det_raw_last) {
        if(s_det_cnt < 0xFFU) {
            s_det_cnt++;
        }
        if(s_det_cnt >= SD_DET_STABLE_N) {
            s_det_stable = now;
            s_det_cnt = 0U;
            return (0U != now) ? BSP_SD_EVT_INSERT : BSP_SD_EVT_REMOVE;
        }
    } else {
        s_det_raw_last = now;
        s_det_cnt = 1U;
    }
    return BSP_SD_EVT_NONE;
}

static void sd_fail(uint8_t stage, sd_error_enum code)
{
    g_sd_fail_stage = stage;
    g_sd_fail_code = (uint32_t)code;
}

/* SDIO DMA 传输期间关掉数码管扫描，避免 TIMER2 抢占/嵌套撑爆栈 */
static void nixie_irq_pause(void)
{
    nvic_irq_disable(BOARD_NIX_SCAN_IRQn);
    timer_disable(BOARD_NIX_SCAN_TIMER);
}

static void nixie_irq_resume(void)
{
    timer_interrupt_flag_clear(BOARD_NIX_SCAN_TIMER, TIMER_INT_FLAG_UP);
    timer_enable(BOARD_NIX_SCAN_TIMER);
    nvic_irq_enable(BOARD_NIX_SCAN_IRQn, 2U, 0U);
}

static void nvic_config(void)
{
    /* 官方：SDIO 抢占优先级 0；须低于 SysTick 嵌套风险已在 systick 侧处理 */
    nvic_irq_enable(SDIO_IRQn, 0U, 0U);
}

static sd_error_enum sd_io_init(void)
{
    sd_error_enum status = SD_OK;
    uint32_t cardstate = 0U;

    status = sd_init();
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_POWER, status);
        return status;
    }

    status = sd_card_information_get(&g_sd_cardinfo);
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_INFO, status);
        return status;
    }

    status = sd_card_select_deselect(g_sd_cardinfo.card_rca);
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_SELECT, status);
        return status;
    }

    status = sd_cardstatus_get(&cardstate);
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_LOCKED, status);
        return status;
    }
    if(0U != (cardstate & 0x02000000U)) {
        sd_fail(BSP_SD_STAGE_LOCKED, SD_LOCK_UNLOCK_FAILED);
        return SD_LOCK_UNLOCK_FAILED;
    }

    status = sd_bus_mode_config(SDIO_BUSMODE_4BIT);
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_BUS, status);
        return status;
    }

    status = sd_transfer_mode_config(SD_DMA_MODE);
    if(SD_OK != status) {
        sd_fail(BSP_SD_STAGE_DMA, status);
        return status;
    }

    g_sd_fail_stage = BSP_SD_STAGE_OK;
    g_sd_fail_code = (uint32_t)SD_OK;
    return SD_OK;
}

uint8_t bsp_sd_init(void)
{
    sd_error_enum sd_error;
    uint16_t i = 5U;

    s_ready = 0U;
    g_sd_fail_stage = 0U;
    g_sd_fail_code = 0U;

    if(0U == s_det_inited) {
        bsp_sd_detect_init();
    }
    delay_1ms(10U);

    if(0U == bsp_sd_is_present()) {
        sd_fail(BSP_SD_STAGE_POWER, SD_CMD_RESP_TIMEOUT);
        return BSP_SD_ERROR;
    }

    nixie_irq_pause();
    nvic_config();

    do {
        sd_error = sd_io_init();
        if(SD_OK != sd_error) {
            (void)sd_power_off();
            delay_1ms(50U);
        }
    } while((SD_OK != sd_error) && (0U != (--i)));

    nixie_irq_resume();

    if(0U == i) {
        return BSP_SD_ERROR;
    }

    s_ready = 1U;
    return BSP_SD_OK;
}

void bsp_sd_deinit(void)
{
    s_ready = 0U;
    nixie_irq_pause();
    (void)sd_power_off();
    nixie_irq_resume();
}

uint32_t bsp_sd_get_capacity_kb(void)
{
    if(0U == s_ready) {
        return 0U;
    }
    return sd_card_capacity_get();
}

uint32_t bsp_sd_get_sector_count(void)
{
    uint32_t kb;

    if(0U == s_ready) {
        return 0U;
    }

    kb = sd_card_capacity_get();
    return kb * 2U;
}

uint8_t bsp_sd_read(uint8_t *pbuf, uint32_t sector, uint32_t cnt)
{
    sd_error_enum status;
    uint32_t addr;

    if((0U == s_ready) || (NULL == pbuf) || (0U == cnt)) {
        return BSP_SD_ERROR;
    }

    addr = sector;
    nixie_irq_pause();

    if(1U == cnt) {
        status = sd_block_read((uint32_t *)pbuf, addr, 512U);
    } else {
        status = sd_multiblocks_read((uint32_t *)pbuf, addr, 512U, cnt);
    }

    nixie_irq_resume();
    return (SD_OK == status) ? BSP_SD_OK : BSP_SD_ERROR;
}

uint8_t bsp_sd_write(uint8_t *pbuf, uint32_t sector, uint32_t cnt)
{
    sd_error_enum status;
    uint32_t addr;

    if((0U == s_ready) || (NULL == pbuf) || (0U == cnt)) {
        return BSP_SD_ERROR;
    }

    addr = sector;
    nixie_irq_pause();

    if(1U == cnt) {
        status = sd_block_write((uint32_t *)pbuf, addr, 512U);
    } else {
        status = sd_multiblocks_write((uint32_t *)pbuf, addr, 512U, cnt);
    }

    nixie_irq_resume();
    return (SD_OK == status) ? BSP_SD_OK : BSP_SD_ERROR;
}
