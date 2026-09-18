#ifndef BOARD_AUDIO_H
#define BOARD_AUDIO_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa a comunicacao com o CS47L63 (Line-In) via SPI
 *        e prepara o recebimento de audio PCM 16-bit via I2S.
 * 
 * @return 0 em caso de sucesso, erro negativo caso contrario
 */
int board_audio_init(void);

/**
 * @brief Retorna a ultima amostra PCM 16-bit lida do I2S (do Line-In)
 * 
 * @return Amostra PCM 16-bit
 */
int16_t board_audio_get_latest_sample(void);

/**
 * @brief Retorna true se o áudio do conector P2 estiver inicializado e pronto
 */
bool board_audio_is_ready(void);

#endif /* BOARD_AUDIO_H */
