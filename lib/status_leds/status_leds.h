/**
 * @file status_leds.h
 * @brief Driver pour WS2812B avec modes d'animation pour l'état robot
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <FastLED.h>

/**
 * @enum LedMode
 * @brief Modes d'animation disponible pour les LEDs
 */
enum class LedMode {
    IDLE,
    MOVING,
    OBSTACLE,
    LISTENING,
    TRACKING,
    ERROR
};

/**
 * @class StatusLeds
 * @brief Gestion des LEDs WS2812B pour affichage d'état
 */
class StatusLeds {
public:
    /**
     * @brief Initialise les LEDs WS2812B
     */
    void begin();

    /**
     * @brief Change le mode d'animation
     * @param mode Mode à appliquer
     */
    void setMode(LedMode mode);

    /**
     * @brief Définit la luminosité globale
     * @param val Valeur 0..255
     */
    void setBrightness(uint8_t val);

    /**
     * @brief Met à jour les animations (appeler dans loop())
     */
    void update();

private:
    static constexpr uint8_t NUM_LEDS = 8;
    static constexpr uint8_t LED_PIN = 4;  // XIAO ESP32S3 D3=GPIO4
    CRGB leds[NUM_LEDS];

    LedMode current_mode = LedMode::IDLE;
    uint8_t brightness = 128;
    uint32_t last_update = 0;
    uint32_t last_mode_change = 0;

    /**
     * @brief Animation IDLE : rotation lente bleu
     */
    void drawIdle();

    /**
     * @brief Animation MOVING : vert fixe, intensité proportionnelle
     * @param speed Niveau de vitesse (0..100)
     */
    void drawMoving(uint8_t speed = 50);

    /**
     * @brief Animation OBSTACLE : rouge clignotant rapide
     */
    void drawObstacle();

    /**
     * @brief Animation LISTENING : pulsation cyan lente
     */
    void drawListening();

    /**
     * @brief Animation TRACKING : orange tournant
     */
    void drawTracking();

    /**
     * @brief Animation ERROR : rouge fixe
     */
    void drawError();
};
