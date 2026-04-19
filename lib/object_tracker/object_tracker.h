/**
 * @file object_tracker.h
 * @brief Suivi d'objet par couleur via caméra OV2640 (RGB565)
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// esp_camera.h inclus UNIQUEMENT dans object_tracker.cpp
// pour éviter le conflit de typedef sensor_t avec Adafruit_Sensor.h

/**
 * @class ObjectTracker
 * @brief Suivi d'objet par couleur avec contrôle PID simplifié
 */
class ObjectTracker {
public:
    /**
     * @brief Initialise la caméra OV2640 QVGA RGB565
     * @return true si initialisation réussie
     */
    bool begin();

    /**
     * @brief Définit la cible de couleur
     * @param r Composante rouge [0-255]
     * @param g Composante verte [0-255]
     * @param b Composante bleue [0-255]
     * @param tol Seuil RGB (0-255)
     */
    void setTargetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t tol);

    /**
     * @brief Analyse une frame et calcule vitesse de suivi
     * @param[out] vx Vitesse longitudinale [-1.0,1.0]
     * @param[out] vy Vitesse latérale [-1.0,1.0]
     * @return true si tracking ok
     */
    bool track(float* vx, float* vy);

    /**
     * @brief Obtient centroid courant
     * @param[out] cx Coordonnée X (0..319)
     * @param[out] cy Coordonnée Y (0..239)
     * @return true si centroid valide
     */
    bool getCentroid(int* cx, int* cy);

    /**
     * @brief Vérifie si objet est en suivi
     * @return true si blob détecté dans la dernière frame
     */
    bool isTracking() { return is_tracking; }

private:
    // camera_config_t déclarée localement dans begin() pour éviter esp_camera.h ici
    uint8_t* frame_buffer    = nullptr;  ///< Buffer PSRAM RGB565
    uint32_t frame_size      = 0;
    TaskHandle_t track_task  = nullptr;
    volatile bool is_tracking = false;

    uint8_t target_r  = 255;  ///< Rouge cible (8-bit)
    uint8_t target_g  = 0;
    uint8_t target_b  = 0;
    uint8_t tolerance = 32;

    int centroid_x     = -1;
    int centroid_y     = -1;
    int centroid_old_x = -1;
    int centroid_old_y = -1;

    static constexpr float K_PROP = 0.003f;  ///< Gain proportionnel (m/s par pixel)

    /**
     * @brief Seuillage couleur sur un pixel RGB565 converti en 8-bit
     */
    bool colorMatch(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Calcul du centroïd du blob coloré dans le buffer
     */
    bool computeCentroid();

    /**
     * @brief Tâche FreeRTOS de capture et de calcul (Core 1, prio 4)
     */
    static void trackTask(void* pvParameters);
};
