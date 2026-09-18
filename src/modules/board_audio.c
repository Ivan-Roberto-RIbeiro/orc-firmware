/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "board_audio.h"
#include "cs47l63_comm.h"
#include "cs47l63_reg_conf.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrfx_i2s.h>
#include <hal/nrf_clock.h>

LOG_MODULE_REGISTER(board_audio, CONFIG_LOG_DEFAULT_LEVEL);

#define HFCLKAUDIO_12_288_MHZ 0x9BA6
#define I2S_BUFFER_SIZE 64

static cs47l63_t cs47l63_driver;
static bool audio_ready = false;

static uint32_t i2s_rx_buffer_a[I2S_BUFFER_SIZE];
static uint32_t i2s_rx_buffer_b[I2S_BUFFER_SIZE];
static volatile int16_t latest_p2_sample = 0;

static nrfx_i2s_t i2s_inst = NRFX_I2S_INSTANCE(NRF_I2S0);

static void i2s_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status)
{
    if (status == NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED && p_released && p_released->p_rx_buffer) {
        /* Extrai a amostra PCM 16-bit do conector P2 Line-In (Canal Esquerdo) */
        uint32_t raw_word = p_released->p_rx_buffer[0];
        latest_p2_sample = (int16_t)(raw_word >> 16);

        /* Alterna o double buffer do DMA */
        nrfx_i2s_buffers_t next_bufs = {
            .p_rx_buffer = (p_released->p_rx_buffer == i2s_rx_buffer_a) ? i2s_rx_buffer_b : i2s_rx_buffer_a,
            .p_tx_buffer = NULL,
            .buffer_size = I2S_BUFFER_SIZE,
        };
        nrfx_i2s_next_buffers_set(&i2s_inst, &next_bufs);
    }
}

int board_audio_init(void)
{
    int ret;
    LOG_INF("Inicializando Conector P2 Soldado (Line-In via CS47L63 + I2S)...");

    /* 1. Inicializa comunicacao SPI com o CS47L63 */
    ret = cs47l63_comm_init(&cs47l63_driver);
    if (ret != 0) {
        LOG_ERR("Falha ao inicializar SPI do CS47L63: %d", ret);
        return ret;
    }

    /* 2. Reseta e configura registradores para o LINE-IN (P2 Soldado) */
    cs47l63_comm_reg_conf_write(soft_reset, ARRAY_SIZE(soft_reset));
    cs47l63_comm_reg_conf_write(clock_configuration, ARRAY_SIZE(clock_configuration));
    cs47l63_comm_reg_conf_write(GPIO_configuration, ARRAY_SIZE(GPIO_configuration));
    cs47l63_comm_reg_conf_write(line_in_enable, ARRAY_SIZE(line_in_enable)); /* Ativa o conector P2 */
    cs47l63_comm_reg_conf_write(asp1_enable, ARRAY_SIZE(asp1_enable));

    /* 3. Inicializa PLL de Audio (HFCLKAUDIO @ 12.288 MHz) */
    nrf_clock_hfclkaudio_config_set(NRF_CLOCK, HFCLKAUDIO_12_288_MHZ);
    nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKAUDIOSTART);

    /* 4. Configura periferico I2S0 da Nordic usando macro padrao */
    nrfx_i2s_config_t i2s_cfg = NRFX_I2S_DEFAULT_CONFIG(14, 16, 12, NRF_I2S_PIN_NOT_CONNECTED, 15);
    i2s_cfg.clksrc = NRF_I2S_CLKSRC_ACLK;
    i2s_cfg.prescalers.ratio = NRF_I2S_RATIO_384X;
    i2s_cfg.channels = NRF_I2S_CHANNELS_STEREO;

    /* Conecta e habilita a interrupcao de hardware do I2S no Zephyr */
    IRQ_CONNECT(I2S0_IRQn, 0, nrfx_i2s_irq_handler, &i2s_inst, 0);
    irq_enable(I2S0_IRQn);

    ret = nrfx_i2s_init(&i2s_inst, &i2s_cfg, i2s_handler);
    if (ret != 0) {
        LOG_ERR("Falha no nrfx_i2s_init: %d", ret);
        return -EIO;
    }

    nrfx_i2s_buffers_t init_bufs = {
        .p_rx_buffer = i2s_rx_buffer_a,
        .p_tx_buffer = NULL,
        .buffer_size = I2S_BUFFER_SIZE,
    };

    ret = nrfx_i2s_start(&i2s_inst, &init_bufs, 0);
    if (ret != 0) {
        LOG_ERR("Falha no nrfx_i2s_start: %d", ret);
        return -EIO;
    }

    audio_ready = true;
    LOG_INF("Conector P2 Soldado (Line-In) inicializado com sucesso!");
    return 0;
}

int16_t board_audio_get_latest_sample(void)
{
    return latest_p2_sample;
}

bool board_audio_is_ready(void)
{
    return audio_ready;
}
