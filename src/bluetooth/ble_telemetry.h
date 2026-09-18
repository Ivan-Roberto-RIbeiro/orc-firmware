/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_TELEMETRY_H_
#define BLE_TELEMETRY_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UUIDs Customizados de 128 bits para o Serviço Tiresias:
 * Serviço:         12345678-1234-5678-1234-56789abcdef0
 * Telemetria (Tx): 12345678-1234-5678-1234-56789abcdef1 (Notify - 208 bytes)
 * Controle (Rx):   12345678-1234-5678-1234-56789abcdef2 (Write - Start/Stop)
 */

#define BT_UUID_TIRESIAS_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define BT_UUID_TIRESIAS_TELEMETRY_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define BT_UUID_TIRESIAS_CTRL_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

#define BT_UUID_TIRESIAS_SERVICE    BT_UUID_DECLARE_128(BT_UUID_TIRESIAS_SERVICE_VAL)
#define BT_UUID_TIRESIAS_TELEMETRY  BT_UUID_DECLARE_128(BT_UUID_TIRESIAS_TELEMETRY_VAL)
#define BT_UUID_TIRESIAS_CTRL       BT_UUID_DECLARE_128(BT_UUID_TIRESIAS_CTRL_VAL)

/* Comandos recebidos via Característica de Controle */
#define BLE_CTRL_CMD_START_TEST     0x01
#define BLE_CTRL_CMD_STOP_TEST      0x02

/* Inicialização do subsistema BLE Telemetry */
int ble_telemetry_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_TELEMETRY_H_ */
