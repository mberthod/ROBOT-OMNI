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
#include "i2c_mutex.h"

/**
 * @brief Initialise MPU6050 et calibre les offsets gyro (100 mesures)
 * @return true si capteur répond, false sinon
 */
bool ImuSensor::begin(TwoWire& wire) {
    // Essai à 0x68 (AD0=GND), puis 0x69 (AD0=HIGH) en fallback
    uint8_t found_addr = 0;
    for (uint8_t addr : {(uint8_t)0x68, (uint8_t)0x69}) {
        wire.beginTransmission(addr);
        if (wire.endTransmission() == 0) {
            found_addr = addr;
            Serial.printf("[IMU] MPU-6050 détecté à 0x%02X\n", addr);
            break;
        }
    }

    if (found_addr == 0) {
        // Tentative de lecture directe du registre WHO_AM_I (0x75) pour diagnostic
        for (uint8_t addr : {(uint8_t)0x68, (uint8_t)0x69}) {
            wire.beginTransmission(addr);
            wire.write(0x75);  // WHO_AM_I register
            wire.endTransmission(false);
            wire.requestFrom(addr, (uint8_t)1);
            if (wire.available()) {
                uint8_t who = wire.read();
                Serial.printf("[IMU] WHO_AM_I à 0x%02X = 0x%02X (attendu 0x68)\n", addr, who);
            } else {
                Serial.printf("[IMU] Aucune réponse WHO_AM_I à 0x%02X\n", addr);
            }
        }

        // Tentative réveil forcé (PWR_MGMT_1 = 0x00 pour sortir du sleep)
        wire.beginTransmission(0x68);
        wire.write(0x6B);  // PWR_MGMT_1
        wire.write(0x00);  // clear SLEEP bit
        uint8_t err = wire.endTransmission();
        Serial.printf("[IMU] Réveil forcé 0x68 → err=%u\n", err);
        if (err == 0) {
            delay(100);
            // Réessayer après réveil
            wire.beginTransmission(0x68);
            if (wire.endTransmission() == 0) {
                found_addr = 0x68;
                Serial.println("[IMU] MPU-6050 trouvé après réveil forcé !");
            }
        }

        if (found_addr == 0) {
            Serial.println("[ERREUR] MPU-6050 introuvable (0x68 et 0x69 testés)");
            Serial.println("[CHECK]  1. VCC → 3.3V (pas 5V)");
            Serial.println("[CHECK]  2. GND → GND");
            Serial.println("[CHECK]  3. SDA → GPIO5 (XIAO D4)");
            Serial.println("[CHECK]  4. SCL → GPIO6 (XIAO D5)");
            Serial.println("[CHECK]  5. AD0 → GND (pour 0x68) ou VCC (pour 0x69)");
            Serial.println("[CHECK]  6. Tester le module seul sur un autre µC");
            return false;
        }
    }

    if (!imu.begin(found_addr, &wire)) {
        Serial.printf("[ERREUR] MPU-6050 init échoué à 0x%02X\n", found_addr);
        return false;
    }

    imu.setAccelerometerRange(MPU6050_RANGE_4_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_260_HZ);

    calibrate();
    last_update = millis();
    Serial.printf("[OK] IMU initialisé @0x%02X\n", found_addr);
    return true;
}

/**
 * @brief Met à jour les angles roll/pitch via filtre complémentaire
 * @note gyro en rad/s → intégration en degrés
 */
void ImuSensor::update() {
    sensors_event_t a, g, temp;
    if (!g_i2c_mutex || !I2C_LOCK(20)) return;
    imu.getEvent(&a, &g, &temp);
    I2C_UNLOCK();

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
    if (!g_i2c_mutex || !I2C_LOCK(20)) return 0.0f;
    imu.getEvent(&a, &g, &temp);
    I2C_UNLOCK();
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
