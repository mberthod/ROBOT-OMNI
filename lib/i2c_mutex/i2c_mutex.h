/**
 * @file i2c_mutex.h
 * @brief Mutex I2C global partagé entre tous les modules
 * @note Thread-safe : TOF task + loop() utilisent le même bus Wire
 *
 * Utilisation :
 *   if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
 *       // opérations I2C ici
 *       xSemaphoreGive(g_i2c_mutex);
 *   }
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/// Mutex global pour le bus I2C — défini dans main.cpp, extern ici
extern SemaphoreHandle_t g_i2c_mutex;

/// Macro utilitaire : exécute bloc seulement si mutex acquis
#define I2C_LOCK(timeout_ms)    (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
#define I2C_UNLOCK()             xSemaphoreGive(g_i2c_mutex)
