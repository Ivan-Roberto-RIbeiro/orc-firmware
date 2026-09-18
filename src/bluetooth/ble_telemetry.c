/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "ble_telemetry.h"
#include "sensor_hub.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_telemetry, CONFIG_LOG_DEFAULT_LEVEL);

#define BLE_TELEMETRY_THREAD_STACK_SIZE 4096
#define BLE_TELEMETRY_THREAD_PRIORITY   7

/* Referencia de memoria do slab de pacotes do sensor_hub */
extern struct k_mem_slab sensor_packet_slab;

/* LED de Erro Critico (apenas acende em caso de falha 0xFFFF) */
#if DT_NODE_EXISTS(DT_ALIAS(error_led))
static const struct gpio_dt_spec error_led_dev = GPIO_DT_SPEC_GET(DT_ALIAS(error_led), gpios);
#define HAS_ERROR_LED 1
#else
#define HAS_ERROR_LED 0
#endif

static struct bt_conn *current_conn = NULL;
static bool notify_enabled = false;

/* =========================================================================
 * Callbacks do Servico GATT
 * ========================================================================= */
static void telemetry_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notificacoes de telemetria: %s", notify_enabled ? "ATIVADAS" : "DESATIVADAS");
}

static ssize_t write_control_handler(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     const void *buf, uint16_t len,
                                     uint16_t offset, uint8_t flags)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(offset);
    ARG_UNUSED(flags);

    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const uint8_t *data = buf;
    uint8_t command = data[0];

    switch (command) {
    case BLE_CTRL_CMD_START_TEST:
        LOG_INF("Comando BLE recebido: [0x01 - INICIAR TESTE]");
        sensor_hub_start();
        break;

    case BLE_CTRL_CMD_STOP_TEST:
        LOG_INF("Comando BLE recebido: [0x02 - PARAR TESTE]");
        sensor_hub_stop();
        break;

    default:
        LOG_WRN("Comando BLE desconhecido: 0x%02X", command);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    return len;
}

/* =========================================================================
 * Declaracao da Tabela GATT
 * ========================================================================= */
BT_GATT_SERVICE_DEFINE(tiresias_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_TIRESIAS_SERVICE),

    /* Caracteristica 1: Telemetria (Notify - 208 bytes condensados) */
    BT_GATT_CHARACTERISTIC(BT_UUID_TIRESIAS_TELEMETRY,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(telemetry_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Caracteristica 2: Controle (Write - Start/Stop de Teste) */
    BT_GATT_CHARACTERISTIC(BT_UUID_TIRESIAS_CTRL,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           NULL, write_control_handler, NULL),
);

/* =========================================================================
 * Callbacks de Conexao e Gerenciamento BLE
 * ========================================================================= */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Falha na conexao BLE: %d", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Central Bluetooth conectada!");

    /* Solicita Data Length Extension (DLE) de ate 251 bytes */
    struct bt_conn_le_data_len_param len_param = {
        .tx_max_len = 251,
        .tx_max_time = 2120,
    };
    int ret = bt_conn_le_data_len_update(current_conn, &len_param);
    if (ret) {
        LOG_DBG("Falha ao solicitar DLE: %d", ret);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);
    LOG_INF("Central desconectada (razao: 0x%02x)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    notify_enabled = false;
    sensor_hub_stop();

#if HAS_ERROR_LED
    if (gpio_is_ready_dt(&error_led_dev)) {
        gpio_pin_set_dt(&error_led_dev, 0); // Garante LED apagado
    }
#endif
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* =========================================================================
 * Dados de Publicidade (Advertising)
 * ========================================================================= */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_TIRESIAS_SERVICE_VAL),
};

/* =========================================================================
 * Thread Consumidora de Telemetria (Baixo Consumo)
 * ========================================================================= */
static void ble_telemetry_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Thread de Telemetria BLE iniciada.");

    while (1) {
        /* Bloqueado em K_FOREVER aguardando dados chegarem na k_fifo */
        struct sensor_packet_t *packet = k_fifo_get(&sensor_data_fifo, K_FOREVER);

        if (packet == NULL) {
            continue;
        }

#if HAS_ERROR_LED
        /* Verifica se ha erro critico para gerenciar o LED da placa */
        bool has_error = (packet->pitch == SENSOR_HUB_ERROR_CODE);
        if (gpio_is_ready_dt(&error_led_dev)) {
            gpio_pin_set_dt(&error_led_dev, has_error ? 1 : 0);
        }
#endif

        /* Envia notificacao GATT se estiver conectado e inscrito */
        if (current_conn && notify_enabled) {
            /* O payload comeca logo apos o campo fifo_reserved */
            const uint8_t *payload_ptr = (const uint8_t *)&packet->sequence_number;

            int ret = bt_gatt_notify(current_conn, &tiresias_svc.attrs[1],
                                     payload_ptr, SENSOR_PACKET_PAYLOAD_SIZE);
            if (ret != 0 && ret != -EAGAIN && ret != -ENOMEM) {
                LOG_DBG("Erro no envio de notificacao: %d", ret);
            }
        }

        /* Devolve o buffer para o slab de memoria */
        k_mem_slab_free(&sensor_packet_slab, (void *)packet);
    }
}

K_THREAD_DEFINE(ble_telemetry_thread_id, BLE_TELEMETRY_THREAD_STACK_SIZE,
                ble_telemetry_thread_entry, NULL, NULL, NULL,
                BLE_TELEMETRY_THREAD_PRIORITY, 0, 0);

/* =========================================================================
 * Inicializacao do BLE
 * ========================================================================= */
int ble_telemetry_init(void)
{
    int ret;

    LOG_INF("Inicializando Bluetooth LE Telemetry...");

#if HAS_ERROR_LED
    if (gpio_is_ready_dt(&error_led_dev)) {
        gpio_pin_configure_dt(&error_led_dev, GPIO_OUTPUT_INACTIVE);
        gpio_pin_set_dt(&error_led_dev, 0);
    }
#endif

    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("bt_enable() falhou: %d", ret);
        return ret;
    }

    ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (ret) {
        LOG_ERR("bt_le_adv_start() falhou: %d", ret);
        return ret;
    }

    LOG_INF("Advertising BLE iniciado como '%s'. Aguardando conexao...", CONFIG_BT_DEVICE_NAME);
    return 0;
}
