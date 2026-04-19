/**
 * @file voice_control.cpp
 * @brief Implémentation du contrôle vocal PDM via driver ESP-IDF I2S natif
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "voice_control.h"
#include <driver/i2s.h>   // Driver I2S ESP-IDF natif (pas la lib Arduino I2SClass)

// Pins PDM microphone XIAO ESP32S3 Sense (connecteur B2B)
#define PDM_CLK_PIN  42
#define PDM_DATA_PIN 41

// Port I2S utilisé pour le micro PDM
#define I2S_PORT     I2S_NUM_0

/**
 * @brief Initialise le driver I2S en mode PDM et alloue le buffer audio en PSRAM
 * @return true si initialisation réussie
 */
bool VoiceControl::begin() {
    // Allocation du buffer audio en PSRAM (1s × 16kHz × 2 octets = 32 Ko)
    audio_buffer = static_cast<int16_t*>(ps_malloc(BUFFER_SIZE));
    if (audio_buffer == nullptr) {
        Serial.printf("[ERREUR] VoiceControl : allocation PSRAM échouée (%u octets)\n", BUFFER_SIZE);
        return false;
    }
    memset(audio_buffer, 0, BUFFER_SIZE);

    // Configuration du driver I2S en mode PDM mono 16 kHz
    const i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 64,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[ERREUR] i2s_driver_install : 0x%x\n", err);
        return false;
    }

    // Assignation des pins PDM
    const i2s_pin_config_t pin_config = {
        .bck_io_num   = I2S_PIN_NO_CHANGE,
        .ws_io_num    = PDM_CLK_PIN,          // PDM CLK sur GPIO42
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = PDM_DATA_PIN,         // PDM DATA sur GPIO41
    };

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[ERREUR] i2s_set_pin : 0x%x\n", err);
        return false;
    }

    Serial.printf("[MIC] PDM I2S OK — CLK=GPIO%d DATA=GPIO%d @%uHz\n",
                  PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE);
    return true;
}

/**
 * @brief Lance la tâche de reconnaissance vocale sur Core 0
 */
void VoiceControl::startListening() {
    if (is_listening || listen_task != nullptr) return;

    is_listening = true;
    xTaskCreatePinnedToCore(
        listenTask,
        "voice_listen",
        8192,
        this,
        5,
        &listen_task,
        0   // Core 0
    );
}

/**
 * @brief Arrête la tâche de reconnaissance vocale
 */
void VoiceControl::stopListening() {
    if (!is_listening || listen_task == nullptr) return;

    is_listening = false;
    vTaskDelay(pdMS_TO_TICKS(50));  // Laisser la tâche se terminer proprement
    vTaskDelete(listen_task);
    listen_task = nullptr;
}

/**
 * @brief Définit la callback pour les commandes reconnues
 * @param cb Callback(command, confidence)
 */
void VoiceControl::setCommandCallback(void (*cb)(const char* command, float confidence)) {
    command_cb = cb;
}

/**
 * @brief Tâche FreeRTOS : lecture PDM via i2s_read() + inférence Edge Impulse
 * @param pvParameters Pointeur vers l'instance VoiceControl
 */
void VoiceControl::listenTask(void* pvParameters) {
    VoiceControl* self = static_cast<VoiceControl*>(pvParameters);
    uint32_t samples_read = 0;

    while (self->is_listening) {
        // Lecture I2S native (octets disponibles dans DMA)
        size_t bytes_read = 0;
        uint8_t* dst = reinterpret_cast<uint8_t*>(self->audio_buffer) + (samples_read * 2);
        size_t to_read = (BUFFER_SAMPLES - samples_read) * 2;

        i2s_read(I2S_PORT, dst, to_read, &bytes_read, pdMS_TO_TICKS(100));
        samples_read += bytes_read / 2;

        // Buffer plein → inférence
        if (samples_read >= BUFFER_SAMPLES) {
            float confidence = self->runEdgeImpulse(self->audio_buffer);
            if (self->command_cb && confidence > 0.5f) {
                self->command_cb("detected", confidence);
            }
            samples_read = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Placeholder pour l'inférence Edge Impulse (à implémenter après import du modèle)
 * @param buf Buffer audio 16 bits
 * @return Confiance maximale (0.0-1.0)
 */
float VoiceControl::runEdgeImpulse(int16_t* buf) {
    // TODO : appeler run_classifier() après import lib/ei_model/
    (void)buf;
    return 0.0f;
}

/**
 * @brief Callback de remplissage I2S (non utilisée avec driver natif)
 */
void VoiceControl::i2sFillCallback(size_t bytes, uint8_t* buf) {
    (void)bytes;
    (void)buf;
}
