#ifndef SENSORES_TEST_H
#define SENSORES_TEST_H

#include <stdint.h>

/**
 * @brief Gera amostras sinteticas simulando sensores (senoides)
 * 
 * @param mic1 Ponteiro para onde salvar a amostra do mic1 (700Hz, amplitude 0.5)
 * @param mic2 Ponteiro para onde salvar a amostra do mic2 (700Hz, amplitude 1.0)
 * @param emg1 Ponteiro para onde salvar a amostra do emg1 (500Hz)
 * @param emg2 Ponteiro para onde salvar a amostra do emg2 (500Hz)
 */
void sensores_test_get_mock_samples(int16_t *mic1, int16_t *mic2, int16_t *emg1, int16_t *emg2);

#endif // SENSORES_TEST_H
