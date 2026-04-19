/**
 * @file imu_sensor.cpp
 * @brief Implémentation du MPU-6050 avec filtre complémentaire
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "imu_sensor.h"
#include <Adafruit_MPU6050.h>
#include <cmath>

/**
 * @brief Initialise MPU6050 et calibre les offsets gyro (100 mesures)
 * @return true si capteur répond, false sinon
 */
bool ImuSensor::begin() {
    if (!imu.begin(I2C_ADDR)) {
        Serial.printf("[ERREUR] MPU-6050 non trouvé à 0x%02X\n", I2C_ADDR);
        return false;
    }
    imu.setAccelerometerRange(MPU6050_RANGE_4_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_260_HZ);

    calibrate();
    last_update = millis();
    return true;
}

/**
 * @brief Met à jour les angles roll/pitch via filtre complémentaire
 * @note gyro en rad/s → intégration en degrés
 */
void ImuSensor::update() {
    sensors_event_t a, g, temp;
    imu.getEvent(&a, &g, &temp);

    // Offsets gyro corrigés (rad/s)
    float gx = g.gyro.x - gyro_offset_x;
    float gy = g.gyro.y - gyro_offset_y;

    // Angles accéléromètre (degrés)
    float acc_roll  = atan2f(a.acceleration.y, a.acceleration.z) * 180.0f / M_PI;
    float acc_pitch = atan2f(-a.acceleration.x,
                              sqrtf(a.acceleration.y * a.acceleration.y +
                                    a.acceleration.z * a.acceleration.z)) * 180.0f / M_PI;

    // Delta temps
    unsigned long now = millis();
    float dt = (now - last_update) / 1000.0f;
    last_update = now;

    // Filtre complémentaire : 98% gyro + 2% accéléromètre
    constexpr float ALPHA = 0.98f;
    roll  = ALPHA * (roll  + gx * dt * 180.0f / M_PI) + (1.0f - ALPHA) * acc_roll;
    pitch = ALPHA * (pitch + gy * dt * 180.0f / M_PI) + (1.0f - ALPHA) * acc_pitch;
}

/**
 * @brief Obtient la vitesse angulaire de lacet (deg/s)
 * @return Taux de yaw corrigé offset
 */
float ImuSensor::getYawRate() {
    sensors_event_t a, g, temp;
    imu.getEvent(&a, &g, &temp);
    return (g.gyro.z - gyro_offset_z) * 180.0f / M_PI;
}

/**
 * @brief Détecte basculement (abs(roll)>45 ou abs(pitch)>45)
 * @return true si robot renversé
 */
bool ImuSensor::isFlipped() {
    return (fabsf(roll) > 45.0f) || (fabsf(pitch) > 45.0f);
}

/**
 * @brief Calcule offsets gyro moyens sur CALIB_MEASURES échantillons
 */
void ImuSensor::calibrate() {
    float gx_sum = 0.0f, gy_sum = 0.0f, gz_sum = 0.0f;

    for (uint8_t i = 0; i < CALIB_MEASURES; i++) {
        sensors_event_t a, g, temp;
        imu.getEvent(&a, &g, &temp);
        gx_sum += g.gyro.x;
        gy_sum += g.gyro.y;
        gz_sum += g.gyro.z;
        delay(10);
    }

    gyro_offset_x = gx_sum / CALIB_MEASURES;
    gyro_offset_y = gy_sum / CALIB_MEASURES;
    gyro_offset_z = gz_sum / CALIB_MEASURES;

    Serial.printf("[CALIB] Gyro offsets X=%+.4f Y=%+.4f Z=%+.4f rad/s\n",
                  gyro_offset_x, gyro_offset_y, gyro_offset_z);
}

/**
 * @brief Filtre complémentaire — non utilisé directement (intégré dans update())
 * @param alpha Pondération gyro (0.0-1.0)
 */
void ImuSensor::applyComplFilter(float alpha) {
    // Logique déplacée dans update() pour éviter une double lecture I2C
    (void)alpha;
}
