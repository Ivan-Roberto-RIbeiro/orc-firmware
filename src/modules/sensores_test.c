#include "sensores_test.h"
#include <math.h>

#define FS 2500.0f
#define PI 3.14159265358979f

static uint32_t sample_index = 0;

void sensores_test_get_mock_samples(int16_t *mic1, int16_t *mic2, int16_t *emg1, int16_t *emg2)
{
    float t = (float)sample_index / FS;

    // --- Sinal EMG (500Hz contínuo) ---
    float f_emg = 500.0f;
    float theta_emg = 2.0f * PI * f_emg * t;
    *emg1 = (int16_t)(32767.0f * sinf(theta_emg));
    *emg2 = (int16_t)(32767.0f * sinf(theta_emg));

    // --- Sinal MIC (Pulso tipo ECG com 700Hz) ---
    // Repete a cada 1 segundo (60 batimentos por minuto)
    float t_cycle = fmodf(t, 1.0f);
    float pulse_duration = 0.05f; // Pulso dura apenas 50 milissegundos

    if (t_cycle < pulse_duration) {
        // Cria um envelope (formato de sino) para o pulso começar e terminar suave
        float envelope = sinf(PI * (t_cycle / pulse_duration));
        
        // A onda interna de 700Hz
        float theta_mic = 2.0f * PI * 700.0f * t;
        float mic_val = envelope * sinf(theta_mic);
        
        *mic1 = (int16_t)(16383.0f * mic_val); // Amplitude 0.5
        *mic2 = (int16_t)(32767.0f * mic_val); // Amplitude 1.0
    } else {
        // Fica em zero no resto do tempo
        *mic1 = 0;
        *mic2 = 0;
    }

    sample_index++;
}
