/**
 * @file object_tracker.cpp
 * @brief Implémentation du suivi caméra couleur OV2640
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

// esp_camera.h inclus ICI SEULEMENT pour éviter conflit sensor_t avec Adafruit
#include <esp_camera.h>
#include "object_tracker.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Pinout OV2640 sur XIAO ESP32S3 Sense (connecteur B2B)
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK     10
#define CAM_PIN_SIOD     40
#define CAM_PIN_SIOC     39
#define CAM_PIN_D7       48
#define CAM_PIN_D6       11
#define CAM_PIN_D5       12
#define CAM_PIN_D4       14
#define CAM_PIN_D3       16
#define CAM_PIN_D2       18
#define CAM_PIN_D1       17
#define CAM_PIN_D0       15
#define CAM_PIN_VSYNC    38
#define CAM_PIN_HREF     47
#define CAM_PIN_PCLK     13

/**
 * @brief Initialise la caméra OV2640 QVGA RGB565
 * @return true si initialisation réussie
 */
bool ObjectTracker::begin() {
    // Allocation PSRAM pour buffer RGB565 320×240
    frame_size   = 320 * 240 * 2;
    frame_buffer = static_cast<uint8_t*>(ps_malloc(frame_size));
    if (frame_buffer == nullptr) {
        Serial.printf("[ERREUR] Échec allocation PSRAM caméra (%lu octets)\n", frame_size);
        return false;
    }

    // Configuration caméra (locale pour ne pas polluer le header)
    camera_config_t config;
    config.ledc_channel  = LEDC_CHANNEL_0;
    config.ledc_timer    = LEDC_TIMER_0;
    config.pin_d0        = CAM_PIN_D0;
    config.pin_d1        = CAM_PIN_D1;
    config.pin_d2        = CAM_PIN_D2;
    config.pin_d3        = CAM_PIN_D3;
    config.pin_d4        = CAM_PIN_D4;
    config.pin_d5        = CAM_PIN_D5;
    config.pin_d6        = CAM_PIN_D6;
    config.pin_d7        = CAM_PIN_D7;
    config.pin_xclk      = CAM_PIN_XCLK;
    config.pin_pclk      = CAM_PIN_PCLK;
    config.pin_vsync     = CAM_PIN_VSYNC;
    config.pin_href      = CAM_PIN_HREF;
    config.pin_sccb_sda  = CAM_PIN_SIOD;
    config.pin_sccb_scl  = CAM_PIN_SIOC;
    config.pin_pwdn      = CAM_PIN_PWDN;
    config.pin_reset     = CAM_PIN_RESET;
    config.xclk_freq_hz  = 20000000;
    config.pixel_format  = PIXFORMAT_RGB565;
    config.frame_size    = FRAMESIZE_QVGA;   // 320×240
    config.jpeg_quality  = 12;
    config.fb_count      = 1;
    config.fb_location   = CAMERA_FB_IN_PSRAM;
    config.grab_mode     = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERREUR] esp_camera_init échoué : 0x%x\n", err);
        return false;
    }
    Serial.printf("[CAM] OV2640 QVGA RGB565 320x240 OK\n");

    // Tâche de suivi sur Core 1, priorité 4, stack 16k
    xTaskCreatePinnedToCore(trackTask, "track", 16384, this, 4, &track_task, 1);
    return true;
}

/**
 * @brief Définit la cible de couleur
 */
void ObjectTracker::setTargetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t tol) {
    target_r  = r;
    target_g  = g;
    target_b  = b;
    tolerance = tol;
}

/**
 * @brief Analyse une frame et calcule vitesse de suivi
 */
bool ObjectTracker::track(float* vx, float* vy) {
    if (!isTracking()) {
        *vx = *vy = 0.0f;
        return false;
    }

    // Erreur relative au centre de l'image (160, 120)
    float err_x = (centroid_x - 160) * K_PROP;
    float err_y = (centroid_y - 120) * K_PROP;

    *vx = -err_y;   // Axe avant/arrière
    *vy = -err_x;   // Axe latéral

    *vx = max(-1.0f, min(1.0f, *vx));
    *vy = max(-1.0f, min(1.0f, *vy));

    return true;
}

/**
 * @brief Obtient centroid courant
 */
bool ObjectTracker::getCentroid(int* cx, int* cy) {
    *cx = centroid_x;
    *cy = centroid_y;
    return isTracking();
}

/**
 * @brief Seuillage couleur — pixel RGB565 converti en 8-bit avant comparaison
 */
bool ObjectTracker::colorMatch(uint8_t r, uint8_t g, uint8_t b) {
    int dr = abs((int)r - (int)target_r);
    int dg = abs((int)g - (int)target_g);
    int db = abs((int)b - (int)target_b);
    return (dr < tolerance) && (dg < tolerance) && (db < tolerance);
}

/**
 * @brief Calcul centroïd blob dans la frame RGB565
 */
bool ObjectTracker::computeCentroid() {
    constexpr uint32_t WIDTH  = 320;
    constexpr uint32_t HEIGHT = 240;
    uint32_t sum_x = 0, sum_y = 0, count = 0;

    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            // Lecture pixel RGB565 little-endian
            const uint8_t* p  = &frame_buffer[(y * WIDTH + x) * 2];
            uint16_t pixel    = (uint16_t)p[0] | ((uint16_t)p[1] << 8);

            // Extraction composantes et conversion 5/6/5 → 8-bit
            uint8_t r = (pixel >> 11) & 0x1F;  r = (r << 3) | (r >> 2);
            uint8_t g = (pixel >>  5) & 0x3F;  g = (g << 2) | (g >> 4);
            uint8_t b =  pixel        & 0x1F;  b = (b << 3) | (b >> 2);

            if (colorMatch(r, g, b)) {
                sum_x += x;
                sum_y += y;
                count++;
            }
        }
    }

    if (count == 0) {
        centroid_x = centroid_y = -1;
        return false;
    }

    centroid_x = static_cast<int>(sum_x / count);
    centroid_y = static_cast<int>(sum_y / count);
    return true;
}

/**
 * @brief Tâche FreeRTOS de capture et de traitement (~30 fps)
 */
void ObjectTracker::trackTask(void* pvParameters) {
    ObjectTracker* self = static_cast<ObjectTracker*>(pvParameters);

    while (1) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb == nullptr) {
            self->is_tracking = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Copie frame vers buffer PSRAM (RGB565 brut)
        if (fb->len <= self->frame_size) {
            memcpy(self->frame_buffer, fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);

        self->is_tracking = self->computeCentroid();

        vTaskDelay(pdMS_TO_TICKS(33));  // ~30 fps
    }
}
