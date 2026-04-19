/**
 * @file mecanum_drive.h
 * @brief Driver pour 4 roues Mecanum via PCA9685 + 2× MX1508
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

/**
 * @class MecanumDrive
 * @brief Contrôleur Mecanum : cinématique cartésienne + polaire via PCA9685
 */
class MecanumDrive {
public:
    /**
     * @brief Initialise le PCA9685 à 1000 Hz (SWRST inclus)
     * @return true si PCA9685 répond sur I2C
     */
    bool begin();

    /**
     * @brief Cinématique Mecanum cartésienne classique
     * @param vx Avant/arrière  [-1.0, +1.0]
     * @param vy Latéral        [-1.0, +1.0]  (+ = droite)
     * @param wz Rotation       [-1.0, +1.0]  (+ = horaire)
     */
    void setVelocity(float vx, float vy, float wz);

    /**
     * @brief Cinématique Mecanum par coordonnées polaires (vecteurs à 45°)
     *
     * Principe : rotation du référentiel de -π/4 rad pour aligner les axes
     * sur les diagonales des roues Mecanum.
     *
     * θ = atan2(jy, jx)          — angle joystick (360°)
     * force = min(||(jx,jy)||, 1) — magnitude clampée à 1
     * P1 = sin(θ - π/4) × force  → FR + RL
     * P2 = cos(θ - π/4) × force  → FL + RR
     *
     * @param jx  Composante latérale joystick  [-1.0, +1.0]  (+ = droite)
     * @param jy  Composante avant/arrière      [-1.0, +1.0]  (+ = avant)
     * @param wz  Rotation                      [-1.0, +1.0]  (+ = horaire)
     */
    void setVelocityPolar(float jx, float jy, float wz);

    /**
     * @brief Coupe tous les canaux (roue libre)
     */
    void stop();

private:
    static constexpr uint8_t  PCA9685_ADDR = 0x40;
    static constexpr uint16_t PWM_FREQ_HZ  = 1000;

    Adafruit_PWMServoDriver pwm{PCA9685_ADDR};
};
