/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "ble_telemetry.h"
#include "sensor_hub.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
    int ret;

    LOG_INF("=================================================");
    LOG_INF(" Firmware Tiresias Sensor Hub (nRF5340) ");
    LOG_INF("=================================================");

    ret = sensor_hub_init();
    if (ret != 0) {
        LOG_ERR("Falha ao inicializar Sensor Hub: %d", ret);
    }

    ret = ble_telemetry_init();
    if (ret != 0) {
        LOG_ERR("Falha ao inicializar BLE Telemetry: %d", ret);
    }

    LOG_INF("Dispositivo anunciado como '%s'.", CONFIG_BT_DEVICE_NAME);
    LOG_INF("Envie [0x01] na caracteristica de Controle para iniciar a transmissao.");

    while (1) {
        k_msleep(5000);
    }

    return 0;
}