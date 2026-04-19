/**
 * @file tof_sensor.cpp
 * @brief VL53L0X avec tâche FreeRTOS + mutex I2C global
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "tof_sensor.h"
#include "i2c_mutex.h"

// ─── Item de queue ─────────────────────────────────────────────────────────────
struct TofQueueItem {
    uint16_t distance_mm;
    bool     valid;
};

// ─── Constructor / Destructor ─────────────────────────────────────────────────
TofSensor::TofSensor()  = default;
TofSensor::~TofSensor() { stop(); }

/**
 * @brief Initialise le VL53L0X et lance la tâche FreeRTOS
 * @note g_i2c_mutex doit être créé AVANT d'appeler begin()
 */
bool TofSensor::begin(TwoWire& wire) {
    sensor.setBus(&wire);
    sensor.setTimeout(TIMEOUT_MS);

    if (!sensor.init()) {
        Serial.println("[ERREUR] VL53L0X : init() échoué");
        return false;
    }

    sensor.setMeasurementTimingBudget(100000);  // 100ms budget (compromis vitesse/précision)
    sensor.startContinuous(50);                 // mesure continue toutes les 50ms
    Serial.printf("[OK] VL53L0X @0x%02X — continu 50ms\n", I2C_ADDR);

    // Queue de longueur 1 : xQueueOverwrite() exige uxLength == 1
    // On veut toujours la mesure la plus fraîche → length=1 est correct
    distance_queue = xQueueCreate(1, sizeof(TofQueueItem));

    // Tâche sur Core 0 (laisse Core 1 pour loop + WiFi)
    xTaskCreatePinnedToCore(
        taskCode,         // fonction
        "tof_task",       // nom
        STACK_SIZE,       // stack (bytes)
        this,             // paramètre = this
        3,                // priorité (3 = sous WiFi, au-dessus loop)
        &task_handle,     // handle
        0                 // Core 0
    );

    return true;
}

/**
 * @brief Lecture depuis la queue (non bloquant, retourne dernière valeur connue)
 */
bool TofSensor::readDistance(uint16_t* distance) {
    TofQueueItem item{};
    // peek = ne retire pas l'item → toujours une valeur disponible
    if (xQueuePeek(distance_queue, &item, pdMS_TO_TICKS(60)) == pdTRUE) {
        *distance = item.distance_mm;
        return item.valid;
    }
    *distance = 9999;
    return false;
}

/**
 * @brief Vérifie obstacle sans bloquer
 */
bool TofSensor::isObstacle(uint16_t threshold_mm) {
    uint16_t dist;
    bool ok = readDistance(&dist);
    bool obstacle = ok && (dist < threshold_mm);
    if (obstacle) {
        Serial.printf("[OBS] TOF=%u mm < seuil %u mm\n", dist, threshold_mm);
    }
    return obstacle;
}

/**
 * @brief Arrête la tâche et libère ressources FreeRTOS
 */
void TofSensor::stop() {
    if (task_handle) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
    if (distance_queue) {
        vQueueDelete(distance_queue);
        distance_queue = nullptr;
    }
}

/**
 * @brief Tâche FreeRTOS Core 0 — lit le capteur et push dans la queue
 * @note Acquiert g_i2c_mutex avant toute lecture I2C
 */
void TofSensor::taskCode(void* pvParameters) {
    auto* self = static_cast<TofSensor*>(pvParameters);

    for (;;) {
        TofQueueItem item{9999, false};

        // Acquérir le mutex I2C partagé avant de toucher le bus
        if (g_i2c_mutex && I2C_LOCK(30)) {
            item.distance_mm = self->sensor.readRangeContinuousMillimeters();
            item.valid       = !self->sensor.timeoutOccurred();
            I2C_UNLOCK();
        }

        // Toujours écrire la valeur fraîche (queue length=1 requise pour Overwrite)
        xQueueOverwrite(self->distance_queue, &item);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
