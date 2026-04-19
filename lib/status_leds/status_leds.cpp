/**
 * @file status_leds.cpp
 * @brief Implémentation des LEDs WS2812B
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "status_leds.h"
#include <FastLED.h>

/**
 * @brief Initialise les LEDs WS2812B
 */
void StatusLeds::begin() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness);
    setMode(LedMode::IDLE);
}

/**
 * @brief Change le mode d'animation
 * @param mode Mode à appliquer
 */
void StatusLeds::setMode(LedMode mode) {
    current_mode = mode;
    last_mode_change = millis();
}

/**
 * @brief Définit la luminosité globale
 * @param val Valeur 0..255
 */
void StatusLeds::setBrightness(uint8_t val) {
    brightness = constrain(val, 10, 255);
    FastLED.setBrightness(brightness);
}

/**
 * @brief Met à jour les animations (appeler dans loop())
 */
void StatusLeds::update() {
    switch (current_mode) {
        case LedMode::IDLE:
            drawIdle();
            break;
        case LedMode::MOVING:
            drawMoving();
            break;
        case LedMode::OBSTACLE:
            drawObstacle();
            break;
        case LedMode::LISTENING:
            drawListening();
            break;
        case LedMode::TRACKING:
            drawTracking();
            break;
        case LedMode::ERROR:
            drawError();
            break;
    }

    FastLED.show();
    last_update = millis();
}

/**
 * @brief Animation IDLE : rotation lente bleu
 */
void StatusLeds::drawIdle() {
    uint32_t now = millis();
    uint8_t offset = (now / 200) % NUM_LEDS;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }

    leds[offset] = CRGB::Blue;
    leds[(offset + 1) % NUM_LEDS] = CRGB::Blue;
}

/**
 * @brief Animation MOVING : vert fixe, intensité proportionnelle
 * @param speed Niveau de vitesse (0..100)
 */
void StatusLeds::drawMoving(uint8_t speed) {
    uint8_t scale = map(speed, 0, 100, 32, 255);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(scale * 2, scale, scale * 2);
    }
}

/**
 * @brief Animation OBSTACLE : rouge clignotant rapide
 */
void StatusLeds::drawObstacle() {
    uint32_t now = millis();
    if ((now / 100) % 2 == 0) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Red;
        }
    } else {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Black;
        }
    }
}

/**
 * @brief Animation LISTENING : pulsation cyan lente
 */
void StatusLeds::drawListening() {
    uint32_t now = millis();
    uint8_t beats = sin8(beat8(20));
    uint8_t val = map(beats, 0, 255, 64, 192);

    CRGB color = CHSV(96, 255, val);  // Cyan

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

/**
 * @brief Animation TRACKING : orange tournant
 */
void StatusLeds::drawTracking() {
    uint32_t now = millis();
    uint8_t offset = (now / 100) % NUM_LEDS;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }

    leds[offset] = CRGB::Orange;
    leds[(offset + 1) % NUM_LEDS] = CRGB::OrangeRed;
    leds[(offset + 2) % NUM_LEDS] = CRGB::Orange;
}

/**
 * @brief Animation ERROR : rouge fixe
 */
void StatusLeds::drawError() {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Red;
    }
}
