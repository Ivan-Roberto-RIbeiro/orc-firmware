/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SENSOR_HUB_H_
#define SENSOR_HUB_H_

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_HUB_SAMPLES_PER_PACKET   25
#define SENSOR_HUB_SAMPLING_RATE_HZ     2500
#define SENSOR_HUB_SAMPLE_INTERVAL_US   (1000000 / SENSOR_HUB_SAMPLING_RATE_HZ) // 400 us
#define SENSOR_HUB_IMU_DIVIDER          50 // 2500 Hz / 50 = 50 Hz (20 ms)

#define SENSOR_HUB_ERROR_CODE           ((int16_t)0xFFFF)

/*
 * Estrutura de uma amostra com os 4 canais analógicos amostrados a 2.500 Hz:
 * - emg1: AIN0 (P0.04) -> Eletrodo EMG 1 (AD8232)
 * - emg2: AIN1 (P0.05) -> Eletrodo EMG 2 (AD8232)
 * - mic1: AIN2 (P0.06) -> Conector P2 Line-In (Entrada de Áudio do PC)
 * - mic2: AIN3 (P0.07) -> Microfone Embutido (On-board MEMS Mic)
 */
struct __packed sensor_sample_t {
    int16_t emg1;   // AIN0 (P0.04) - EMG 1
    int16_t emg2;   // AIN1 (P0.05) - EMG 2
    int16_t mic1;   // AIN2 (P0.06) - P2 Line-In (Áudio PC)
    int16_t mic2;   // AIN3 (P0.07) - Microfone Embutido da Placa
};

/* Estrutura do pacote de telemetria transmitido via BLE (208 bytes) */
struct __packed sensor_packet_t {
    void *fifo_reserved;                                    // Reservado para uso interno da k_fifo
    uint32_t sequence_number;                               // Contador sequencial de pacotes
    int16_t  pitch;                                         // Pitch BMI270 a 50 Hz (graus * 100)
    int16_t  roll;                                          // Roll BMI270 a 50 Hz (graus * 100)
    struct sensor_sample_t samples[SENSOR_HUB_SAMPLES_PER_PACKET]; // 25 amostras x 8 bytes = 200 bytes
};

/* Tamanho do payload BLE efetivo (sem o ponteiro da k_fifo) */
#define SENSOR_PACKET_PAYLOAD_SIZE (sizeof(struct sensor_packet_t) - sizeof(void *))

/* Inicialização do subsistema de sensores */
int sensor_hub_init(void);

/* Controle de início e parada de aquisição */
int sensor_hub_start(void);
int sensor_hub_stop(void);

/* Status da aquisição */
bool sensor_hub_is_running(void);

/* FIFO de comunicação entre o Sensor Hub e a Thread BLE */
extern struct k_fifo sensor_data_fifo;

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HUB_H_ */
