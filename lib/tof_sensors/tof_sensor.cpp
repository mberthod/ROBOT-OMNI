/**
 * @file tof_sensor.cpp
 * @brief Implémentation VL53L0X (1 seul capteur, XSHUT tiré à 3V3)
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "tof_sensor.h"

/**
 * @brief Initialise le VL53L0X sur le Wire fourni
 * @param wire Bus I2C configuré dans main.cpp
 * @return true si capteur initialisé
 */
bool TofSensor::begin(TwoWire& wire) {
    // XSHUT = pull-up 3V3 permanent → pas de reset GPIO nécessaire
    sensor.setBus(&wire);
    sensor.setTimeout(TIMEOUT_MS);

    if (!sensor.init()) {
        Serial.println("[ERREUR] VL53L0X : init() échoué");
        Serial.println("[CHECK]  VCC=3V3, SDA=GPIO5, SCL=GPIO6, adresse 0x29 ?");
        return false;
    }

    // Haute précision : temps de mesure 200ms (meilleure résolution ±3%)
    sensor.setMeasurementTimingBudget(200000);  // µs

    // Mesure continue toutes les 50ms
    sensor.startContinuous(50);

    Serial.printf("[OK] VL53L0X @0x%02X — continu 50ms, budget 200ms\n", I2C_ADDR);
    return true;
}

/**
 * @brief Lit la distance du capteur
 * @param[out] distance Distance en mm
 * @return true si lecture valide (pas de timeout)
 */
bool TofSensor::readDistance(uint16_t* distance) {
    *distance = sensor.readRangeContinuousMillimeters();

    if (sensor.timeoutOccurred()) {
        Serial.println("[ERREUR] VL53L0X : timeout lecture");
        *distance = 9999;
        return false;
    }
    return true;
}

/**
 * @brief Vérifie si obstacle détecté sous le seuil
 * @param threshold_mm Seuil en mm
 * @return true si obstacle détecté
 */
bool TofSensor::isObstacle(uint16_t threshold_mm) {
    uint16_t dist;
    if (!readDistance(&dist)) return false;

    bool obstacle = (dist < threshold_mm);
    if (obstacle) {
        Serial.printf("[OBS] distance=%u mm < seuil %u mm\n", dist, threshold_mm);
    }
    return obstacle;
}
