/**
 * @file tof_sensor.h
 * @brief Driver pour 1 capteur VL53L0X ToF (sans XSHUT, pull-up 3V3)
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

/**
 * @class TofSensor
 * @brief Gestion d'un VL53L0X à l'adresse par défaut 0x29
 * @note XSHUT tiré à 3V3 en permanence — pas de gestion de reset
 */
class TofSensor {
public:
    /**
     * @brief Initialise le capteur sur le bus Wire fourni
     * @param wire Bus I2C configuré dans main.cpp (GPIO5/6)
     * @return true si capteur initialisé
     */
    bool begin(TwoWire& wire = Wire);

    /**
     * @brief Lit la distance du capteur
     * @param[out] distance Distance en mm
     * @return true si lecture valide (pas de timeout)
     */
    bool readDistance(uint16_t* distance);

    /**
     * @brief Vérifie si obstacle détecté sous le seuil
     * @param threshold_mm Seuil en mm
     * @return true si obstacle détecté
     */
    bool isObstacle(uint16_t threshold_mm);

private:
    VL53L0X sensor;
    static constexpr uint8_t I2C_ADDR   = 0x29;  // adresse par défaut VL53L0X
    static constexpr uint16_t TIMEOUT_MS = 500;
};
