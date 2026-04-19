/**
 * @file imu_sensor.h
 * @brief Driver pour MPU-6050 avec calibration gyro et calcul angles
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <Adafruit_MPU6050.h>

/**
 * @class ImuSensor
 * @brief Gestion du MPU-6050 avec calibration gyro et filtre complémentaire
 */
class ImuSensor {
public:
    /**
     * @brief Initialise MPU6050 et calibre les offsets gyro (100 mesures)
     * @return true si capteur répond, false sinon
     */
    bool begin();

    /**
     * @brief Met à jour les angles roll/pitch via filtre complémentaire
     */
    void update();

    /**
     * @brief Obtient l'angle roll en degrés
     * @return Angle roll (degrés)
     */
    float getRoll() { return roll; }

    /**
     * @brief Obtient l'angle pitch en degrés
     * @return Angle pitch (degrés)
     */
    float getPitch() { return pitch; }

    /**
     * @brief Obtient la vitesse angulaire au sol (deg/s)
     * @return Taux de yaw corrigé
     */
    float getYawRate();

    /**
     * @brief Détecte basculement (abs(roll)>45 ou abs(pitch)>45)
     * @return true si robot renversé
     */
    bool isFlipped();

private:
    Adafruit_MPU6050 imu;
    static constexpr uint8_t I2C_ADDR = 0x68;
    static constexpr uint8_t CALIB_MEASURES = 100;

    // Offsets calculés
    float gyro_offset_x = 0.0f;
    float gyro_offset_y = 0.0f;
    float gyro_offset_z = 0.0f;

    // État filtre
    float roll = 0.0f;
    float pitch = 0.0f;
    float roll_acc = 0.0f;
    float pitch_acc = 0.0f;
    unsigned long last_update = 0;

    /**
     * @brief Calcule offsets gyro moyens
     */
    void calibrate();

    /**
     * @brief Filtre complémentaire simple pour fusion acc/gyro
     * @param alpha Ponderation acc (0.0-1.0)
     */
    void applyComplFilter(float alpha);
};
