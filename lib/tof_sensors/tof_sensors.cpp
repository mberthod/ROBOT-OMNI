/**
 * @file tof_sensors.cpp
 * @brief Implémentation du capteur ToF VL53L1X (1 seul capteur)
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "tof_sensors.h"
#include <VL53L1X.h>
#include <Wire.h>

/**
 * @brief Initialise le capteur à l'adresse 0x30
 * @return true si capteur initialisé
 */
bool TofSensor::begin() {
    pinMode(XSHUT_PIN, OUTPUT);
    digitalWrite(XSHUT_PIN, HIGH);
    delay(10);

    // Bind du bus I2C
    sensor.setBus(&Wire);
    sensor.setTimeout(500);

    // Init à l'adresse par défaut, puis changement vers 0x30
    if (!sensor.init()) {
        return false;
    }
    sensor.setAddress(I2C_ADDR);

    // Mode longue distance, période 50ms
    sensor.setDistanceMode(VL53L1X::Long);
    sensor.startContinuous(50);

    return true;
}

/**
 * @brief Lit la distance du capteur
 * @param[out] distance Distance en mm
 * @return true si lecture réussie
 */
bool TofSensor::readDistance(uint16_t* distance) {
    *distance = sensor.read();
    if (sensor.ranging_data.range_status != 0) {
        Serial.printf("[ERREUR] TOF front status=%d\n", sensor.ranging_data.range_status);
        return false;
    }
    return true;
}

/**
 * @brief Vérifie si obstacle détecté
 * @param threshold_mm Seuil en mm
 * @return true si obstacle détecté
 */
bool TofSensor::isObstacle(uint16_t threshold_mm) {
    uint16_t dist;
    if (!readDistance(&dist)) {
        return false;
    }

    bool obstacle = (dist < threshold_mm);
    if (obstacle) {
        Serial.printf("[OBS] distance=%u < seuil %u mm\n", dist, threshold_mm);
    }

    return obstacle;
}


    return ok;
}

/**
 * @brief Lit les distances de tous les capteurs
 * @param[out] front Distance frontale (TOF1)
 * @param[out] left Distance latérale gauche (TOF2)
 * @param[out] right Distance latérale droit (TOF3)
 * @return true si toutes les lectures sont valides
 */
bool TofSensors::readAll(uint16_t* front, uint16_t* left, uint16_t* right) {
    // read() remplit ranging_data et retourne range_mm (bloquant si pas de timeout)
    *front = sensors[0].read();
    *left  = sensors[1].read();
    *right = sensors[2].read();

    // range_status == 0 signifie mesure valide dans la lib Pololu
    bool success = true;
    if (sensors[0].ranging_data.range_status != 0) {
        Serial.printf("[ERREUR] TOF front status=%d\n", sensors[0].ranging_data.range_status);
        success = false;
    }
    if (sensors[1].ranging_data.range_status != 0) {
        Serial.printf("[ERREUR] TOF left  status=%d\n", sensors[1].ranging_data.range_status);
        success = false;
    }
    if (sensors[2].ranging_data.range_status != 0) {
        Serial.printf("[ERREUR] TOF right status=%d\n", sensors[2].ranging_data.range_status);
        success = false;
    }

    return success;
}

/**
 * @brief Détecte obstacle (un capteur en dessous du seuil)
 * @param threshold_mm Seuil en mm
 * @return true si obstacle détecté
 */
bool TofSensors::isObstacle(uint16_t threshold_mm) {
    uint16_t front, left, right;
    if (!readAll(&front, &left, &right)) {
        return false;
    }

    bool obstacle = (front < threshold_mm) || (left < threshold_mm) || (right < threshold_mm);

    if (obstacle) {
        Serial.printf("[OBS] front=%u left=%u right=%u < seuil %u mm\n",
                      front, left, right, threshold_mm);
    }

    return obstacle;
}

/**
 * @brief Initialise un capteur unique via XSHUT puis change son adresse I2C
 * @param sensor Index du capteur (0,1,2)
 * @param addr Adresse I2C cible
 * @param xshut Pin XSHUT
 * @return true si capteur initialisé
 */
bool TofSensors::initSingle(uint8_t sensor, uint8_t addr, uint8_t xshut) {
    // Réveil du capteur (LOW→HIGH)
    digitalWrite(xshut, HIGH);
    delay(10);

    // Bind du bus I2C
    sensors[sensor].setBus(&Wire);
    sensors[sensor].setTimeout(500);

    // Init à l'adresse par défaut 0x29
    if (!sensors[sensor].init()) {
        Serial.printf("[ERREUR] TOF %d (addr cible 0x%02X) introuvable\n", sensor, addr);
        return false;
    }

    // Changement d'adresse vers addr
    sensors[sensor].setAddress(addr);

    // Mode longue distance, période 50ms
    sensors[sensor].setDistanceMode(VL53L1X::Long);
    sensors[sensor].startContinuous(50);

    Serial.printf("[TOF] Capteur %d initialisé @ 0x%02X\n", sensor, addr);
    return true;
}
