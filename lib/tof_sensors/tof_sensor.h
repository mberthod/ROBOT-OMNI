/**
 * @file tof_sensor.h
 * @brief Driver VL53L0X — tâche FreeRTOS + mutex I2C global
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <VL53L0X.h>

/**
 * @class TofSensor
 * @brief VL53L0X sur tâche FreeRTOS Core 0 avec mutex I2C partagé
 * @note readDistance() est non-bloquant (peek sur queue)
 */
class TofSensor {
public:
    TofSensor();
    ~TofSensor();

    /**
     * @brief Initialise le capteur et lance la tâche (Core 0, priorité 3)
     * @param wire Bus I2C configuré dans main.cpp
     * @return true si init réussi
     */
    bool begin(TwoWire& wire = Wire);

    /**
     * @brief Lit la dernière distance mesurée (non-bloquant)
     * @param[out] distance Distance en mm
     * @return true si mesure valide
     */
    bool readDistance(uint16_t* distance);

    /**
     * @brief Détecte obstacle sous le seuil
     * @param threshold_mm Seuil en mm
     */
    bool isObstacle(uint16_t threshold_mm);

    /**
     * @brief Arrête la tâche et libère ressources
     */
    void stop();

private:
    VL53L0X sensor;

    static constexpr uint8_t  I2C_ADDR   = 0x29;
    static constexpr uint16_t TIMEOUT_MS = 500;
    static constexpr uint32_t STACK_SIZE = 3072;

    QueueHandle_t distance_queue = nullptr;
    TaskHandle_t  task_handle    = nullptr;

    /// @brief Tâche FreeRTOS de lecture continue
    static void taskCode(void* pvParameters);
};
