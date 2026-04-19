/**
 * @file status_leds.cpp
 * @brief Animations WS2812B — visuellement distinctives par mode
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "status_leds.h"
#include <FastLED.h>

void StatusLeds::begin() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

void StatusLeds::setMode(LedMode mode) {
    if (mode != current_mode) {
        current_mode     = mode;
        last_mode_change = millis();
        // Effacer immédiatement pour transition nette
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
}

void StatusLeds::setBrightness(uint8_t val) {
    brightness = constrain(val, 10, 255);
    FastLED.setBrightness(brightness);
}

void StatusLeds::update() {
    switch (current_mode) {
        case LedMode::IDLE:      drawIdle();      break;
        case LedMode::MOVING:    drawMoving();    break;
        case LedMode::OBSTACLE:  drawObstacle();  break;
        case LedMode::LISTENING: drawListening(); break;
        case LedMode::TRACKING:  drawTracking();  break;
        case LedMode::ERRORTOF:  drawErrorTof();  break;
        case LedMode::ERRORIMU:  drawErrorImu();  break;
        case LedMode::ERROR:     drawError();     break;
    }
    FastLED.show();
}

// ─── IDLE : pulsation bleue lente (respiration) ──────────────────────────────
void StatusLeds::drawIdle() {
    // Respiration : sin8 sur 4 secondes cycle
    uint8_t breath = sin8(millis() / 16);   // 0→255 en ~4s
    uint8_t val    = map(breath, 0, 255, 20, 200);
    fill_solid(leds, NUM_LEDS, CHSV(160, 255, val));  // bleu
}

// ─── MOVING : vert plein fixe ────────────────────────────────────────────────
void StatusLeds::drawMoving(uint8_t speed) {
    uint8_t bright = map(speed, 0, 100, 80, 255);
    fill_solid(leds, NUM_LEDS, CHSV(96, 255, bright)); // vert
}

// ─── OBSTACLE : stroboscope rouge très rapide ─────────────────────────────────
// 50ms ON / 50ms OFF — bien plus visible que 100ms
void StatusLeds::drawObstacle() {
    uint32_t now = millis();
    if ((now / 50) % 2 == 0) {
        fill_solid(leds, NUM_LEDS, CRGB::Red);
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
}

// ─── LISTENING : pulsation cyan lente ────────────────────────────────────────
void StatusLeds::drawListening() {
    uint8_t breath = sin8(millis() / 8);   // cycle ~2s
    uint8_t val    = map(breath, 0, 255, 40, 220);
    fill_solid(leds, NUM_LEDS, CHSV(128, 255, val)); // cyan
}

// ─── TRACKING : comète orange tournante ──────────────────────────────────────
void StatusLeds::drawTracking() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    uint8_t head = (millis() / 80) % NUM_LEDS;   // tour en ~640ms
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t idx    = (head + NUM_LEDS - i) % NUM_LEDS;
        uint8_t bright = 255 - (i * 55);
        leds[idx] = CHSV(20, 255, bright);  // orange décroissant
    }
}

// ─── ERRORTOF : violet SOS (3 courts 3 longs 3 courts) ───────────────────────
void StatusLeds::drawErrorTof() {
    // Clignotement irrégulier à 150ms pour distinguer de ERROR (rouge fixe)
    uint32_t now = millis();
    if ((now / 150) % 2 == 0) {
        fill_solid(leds, NUM_LEDS, CRGB::Purple);
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
}

// ─── ERRORIMU : orange clignotant lent ───────────────────────────────────────
void StatusLeds::drawErrorImu() {
    uint32_t now = millis();
    if ((now / 400) % 2 == 0) {
        fill_solid(leds, NUM_LEDS, CRGB::OrangeRed);
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
}

// ─── ERROR : rouge fixe (halt système) ───────────────────────────────────────
void StatusLeds::drawError() {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
}
