/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sensor_hub.h"
#include "sensores_test.h"
#include "board_audio.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <hal/nrf_saadc.h>

LOG_MODULE_REGISTER(sensor_hub, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *adc_dev;
static const struct device *bmi270_dev;

static bool is_streaming = false;
static uint32_t packet_counter = 0;

static bool bmi270_available = false;
static bool adc_available = false;

static int16_t raw_adc_buffer[4];
static struct adc_sequence adc_seq = {
    .buffer = raw_adc_buffer,
    .buffer_size = sizeof(raw_adc_buffer),
};

K_MEM_SLAB_DEFINE(sensor_packet_slab, sizeof(struct sensor_packet_t), 10, 4);
K_FIFO_DEFINE(sensor_data_fifo);

#define SENSOR_HUB_STACK_SIZE 2048
#define SENSOR_HUB_PRIORITY   5

static K_THREAD_STACK_DEFINE(sensor_hub_stack, SENSOR_HUB_STACK_SIZE);
static struct k_thread sensor_hub_thread_data;
void sensor_hub_thread(void *p1, void *p2, void *p3);

int sensor_hub_init(void)
{
    LOG_INF("Inicializando Sensor Hub...");

    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC nao esta pronto.");
        adc_available = false;
    } else {
        adc_available = true;
        struct adc_channel_cfg ch0_cfg = {
            .gain             = ADC_GAIN_1_6,
            .reference        = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,
            .channel_id       = 0,
            .input_positive   = NRF_SAADC_INPUT_AIN0,
        };
        adc_channel_setup(adc_dev, &ch0_cfg);

        struct adc_channel_cfg ch1_cfg = {
            .gain             = ADC_GAIN_1_6,
            .reference        = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,
            .channel_id       = 1,
            .input_positive   = NRF_SAADC_INPUT_AIN1,
        };
        adc_channel_setup(adc_dev, &ch1_cfg);

        adc_seq.channels = BIT(0) | BIT(1);
    }

    board_audio_init();

    bmi270_dev = DEVICE_DT_GET_ANY(bosch_bmi270);
    if (!device_is_ready(bmi270_dev)) {
        LOG_WRN("BMI270 nao detectado no I2C. Telemetria IMU enviara 0xFFFF.");
        bmi270_available = false;
    } else {
        bmi270_available = true;
        LOG_INF("IMU Bosch BMI270 detectado e pronto.");
        
        struct sensor_value attr;
        attr.val1 = 100;
        attr.val2 = 0;
        sensor_attr_set(bmi270_dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &attr);
    }

    k_thread_create(&sensor_hub_thread_data, sensor_hub_stack,
                    K_THREAD_STACK_SIZEOF(sensor_hub_stack),
                    (k_thread_entry_t)sensor_hub_thread,
                    NULL, NULL, NULL,
                    SENSOR_HUB_PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(&sensor_hub_thread_data, "SensorHub_Thread");
    return 0;
}

void sensor_hub_thread(void *p1, void *p2, void *p3)
{
    struct sensor_packet_t *curr_packet = NULL;
    uint8_t sample_idx = 0;
    uint32_t imu_subsample_counter = 0;
    struct sensor_value accel[3];

    while (1) {
        if (!is_streaming) {
            k_msleep(20);
            continue;
        }

        if (curr_packet == NULL) {
            if (k_mem_slab_alloc(&sensor_packet_slab, (void **)&curr_packet, K_NO_WAIT) != 0) {
                LOG_WRN("Memoria do Sensor Hub cheia (descarte)");
                k_usleep(SENSOR_HUB_SAMPLE_INTERVAL_US);
                continue;
            }
        }

        if (sample_idx == 0) {
            curr_packet->sequence_number = packet_counter++;

            if (bmi270_available && (imu_subsample_counter++ % SENSOR_HUB_IMU_DIVIDER == 0)) {
                sensor_sample_fetch(bmi270_dev);
                sensor_channel_get(bmi270_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
                curr_packet->pitch = (int16_t)(sensor_value_to_double(&accel[0]) * 100);
                curr_packet->roll  = (int16_t)(sensor_value_to_double(&accel[1]) * 100);
            } else if (!bmi270_available) {
                curr_packet->pitch = SENSOR_HUB_ERROR_CODE;
                curr_packet->roll  = SENSOR_HUB_ERROR_CODE;
            }
        }

        int16_t mock_mic1, mock_mic2, mock_emg1, mock_emg2;
        sensores_test_get_mock_samples(&mock_mic1, &mock_mic2, &mock_emg1, &mock_emg2);

        curr_packet->samples[sample_idx].emg1 = mock_emg1;
        curr_packet->samples[sample_idx].emg2 = mock_emg2;
        
        /* Conecta o áudio REAL do Cirrus Logic (Jack P2) no canal MIC1 */
        curr_packet->samples[sample_idx].mic1 = board_audio_get_latest_sample();
        curr_packet->samples[sample_idx].mic2 = mock_mic2;

        sample_idx++;

        if (sample_idx >= SENSOR_HUB_SAMPLES_PER_PACKET) {
            k_fifo_put(&sensor_data_fifo, curr_packet);
            curr_packet = NULL;
            sample_idx = 0;
        }

        k_usleep(SENSOR_HUB_SAMPLE_INTERVAL_US);
    }
}

int sensor_hub_start(void) { packet_counter = 0; is_streaming = true; return 0; }
int sensor_hub_stop(void) { is_streaming = false; return 0; }
bool sensor_hub_is_running(void) { return is_streaming; }