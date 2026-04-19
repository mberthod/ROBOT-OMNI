/**
 * @file voice_control.h
 * @brief Reconnaissance vocale via Edge Impulse sur micro PDM
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @class VoiceControl
 * @brief Contrôle vocal avec Edge Impulse sur micro PDM
 */
class VoiceControl {
public:
    /**
     * @brief Initialise I2S PDM et buffer audio (PSRAM)
     * @return true si initialisation réussie
     */
    bool begin();

    /**
     * @brief Lance la tâche de reconnaissance vocale sur Core 0
     */
    void startListening();

    /**
     * @brief Arrête la tâche de reconnaissance vocale
     */
    void stopListening();

    /**
     * @brief Définit la callback pour les commandes reconnues
     * @param cb Callback(command, confidence)
     */
    void setCommandCallback(void (*cb)(const char* command, float confidence));

private:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t BUFFER_SAMPLES = SAMPLE_RATE;  // 1s
    static constexpr uint32_t BUFFER_SIZE = BUFFER_SAMPLES * 2;  // 16 bits

    int16_t* audio_buffer = nullptr;
    TaskHandle_t listen_task = nullptr;
    volatile bool is_listening = false;
    void (*command_cb)(const char* command, float confidence) = nullptr;

    /**
     * @brief Tâche FreeRTOS pour reconnaissance vocale
     * @param pvParameters Non utilisé
     */
    static void listenTask(void* pvParameters);

    /**
     * @brief Callback de remplissage buffer I2S
     * @param bytes Nombre d'octets disponibles
     * @param buf Buffer audio 16 bits
     */
    static void i2sFillCallback(size_t bytes, uint8_t* buf);

    /**
     * @brief Traitement Edge Impulse
     * @param buf Buffer audio
     * @return Confiance (0.0-1.0)
     */
    float runEdgeImpulse(int16_t* buf);
};
