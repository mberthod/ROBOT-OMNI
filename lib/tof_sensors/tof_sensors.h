/**
 * @file tof_sensors.h
 * @brief Driver pour 1 capteur VL53L1X ToF avec gestion XSHUT
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <VL53L1X.h>

/**
 * @class TofSensor
 * @brief Gestion de 1 capteur VL53L1X
 */
class TofSensor {
public:
    /**
     * @brief Initialise le capteur à l'adresse 0x30
     * @return true si capteur initialisé
     */
    bool begin();

    /**
     * @brief Lit la distance du capteur
     * @param[out] distance Distance en mm
     * @return true si lecture réussie
     */
    bool readDistance(uint16_t* distance);

    /**
     * @brief Vérifie si obstacle détecté
     * @param threshold_mm Seuil en mm
     * @return true si obstacle détecté
     */
    bool isObstacle(uint16_t threshold_mm);

private:
    VL53L1X sensor;
    static constexpr uint8_t XSHUT_PIN = 1;  // GPIO1 pour le XSHUT
    static constexpr uint8_t I2C_ADDR = 0x30;
};

