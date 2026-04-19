/**
 * @file main.cpp
 * @brief Fichier principal ROBO-OMNI : contrôle global et boucle principale
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include <Arduino.h>
#include "mecanum_drive.h"
#include "tof_sensor.h"  // Un seul capteur TOF (tof_sensor.cpp)
#include "imu_sensor.h"
#include "status_leds.h"
#include "web_server.h"
#include "wifi_location.h"
#include "voice_control.h"
#include "object_tracker.h"

// ─── Table d'identification des devices I2C connus ────────────────────────────
struct I2cDevice {
    uint8_t addr;
    const char* name;
};

static const I2cDevice KNOWN_DEVICES[] = {
    { 0x29, "VL53L1X (TOF, adresse defaut)"      },
    { 0x30, "VL53L1X TOF #1"                     },
    { 0x31, "VL53L1X TOF #2"                     },
    { 0x32, "VL53L1X TOF #3"                     },
    { 0x40, "PCA9685 (moteurs)"                   },
    { 0x41, "PCA9685 (A0=1)"                      },
    { 0x48, "ADS1115 / TMP102"                    },
    { 0x57, "MAX30102 (SpO2)"                     },
    { 0x60, "MPL3115 (baro)"                      },
    { 0x68, "MPU-6050 IMU (AD0=GND)"             },
    { 0x69, "MPU-6050 IMU (AD0=VCC)"             },
    { 0x70, "PCA9685 ALL CALL / TCA9548A"         },
    { 0x76, "BMP280 / BME280 (SDO=GND)"          },
    { 0x77, "BMP280 / BME280 (SDO=VCC)"          },
};

static const char* i2c_identify(uint8_t addr) {
    for (const auto& d : KNOWN_DEVICES) {
        if (d.addr == addr) return d.name;
    }
    return "device inconnu";
}

/**
 * @brief Scan I2C complet 0x00→0x7F avec tableau visuel et identification
 */
static void i2c_scan() {
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println(  "║         SCAN I2C  0x00 → 0x7F           ║");
    Serial.println(  "╠══════════════════════════════════════════╣");

    // En-tête colonne
    Serial.print("     ");
    for (uint8_t col = 0; col < 16; col++) {
        Serial.printf(" .%X", col);
    }
    Serial.println();

    uint8_t found = 0;
    char found_list[32][32];  // max 32 devices

    for (uint8_t row = 0; row < 8; row++) {
        Serial.printf("0x%X0 ", row);
        for (uint8_t col = 0; col < 16; col++) {
            uint8_t addr = (row << 4) | col;

            // Adresses réservées I2C (0x00–0x07 et 0x78–0x7F)
            if (addr < 0x08 || addr > 0x77) {
                Serial.print("  --");
                continue;
            }

            Wire.beginTransmission(addr);
            uint8_t err = Wire.endTransmission();

            if (err == 0) {
                Serial.printf("  %02X", addr);
                snprintf(found_list[found], 32, "0x%02X", addr);
                found++;
            } else {
                Serial.print("  ..");
            }
        }
        Serial.println();
    }

    Serial.println("╠══════════════════════════════════════════╣");

    if (found == 0) {
        Serial.println("║  ⚠️  AUCUN device trouvé                 ║");
        Serial.println("║  Vérifier SDA=GPIO5 SCL=GPIO6 et alim   ║");
    } else {
        Serial.printf( "║  %u device(s) détecté(s) :               ║\n", found);
        Serial.println("╠══════════════════════════════════════════╣");
        for (uint8_t i = 0; i < found; i++) {
            // Re-scanner pour récupérer l'adresse (depuis found_list)
            uint8_t addr = (uint8_t)strtol(found_list[i], nullptr, 16);
            Serial.printf("║  %-5s → %-32s║\n", found_list[i], i2c_identify(addr));
        }
    }

    Serial.println("╚══════════════════════════════════════════╝\n");
}

// ─── Config réseau ────────────────────────────────────────────────────────────
static constexpr char WIFI_SSID[]     = "TP-Link_B150";
static constexpr char WIFI_PASSWORD[] = "12902637";

// ─── Flags hardware présent ───────────────────────────────────────────────────
// Passer à true quand le composant est branché et initialisé avec succès
static bool hw_tof     = false;
static bool hw_imu     = false;
static bool hw_camera  = false;
static bool hw_voice   = false;

// ─── Instances globales ───────────────────────────────────────────────────────
static MecanumDrive  mecanum;
static TofSensor    tof;  // Un seul capteur
static ImuSensor     imu;
static StatusLeds    leds;
static WebServer     web;
static WifiLocation  wifi_loc;
static VoiceControl  voice;
static ObjectTracker tracker;

// ─── Callback commande vocale ─────────────────────────────────────────────────
static void voiceCb(const char* cmd, float conf) {
    if (conf < 0.5f) return;
    float vx = 0.0f, vy = 0.0f, wz = 0.0f;
    if      (strcmp(cmd, "forward")  == 0) vx =  0.3f;
    else if (strcmp(cmd, "backward") == 0) vx = -0.3f;
    else if (strcmp(cmd, "left")     == 0) vy = -0.25f;
    else if (strcmp(cmd, "right")    == 0) vy =  0.25f;
    else if (strcmp(cmd, "stop")     == 0) { vx = vy = wz = 0.0f; }
    else if (strcmp(cmd, "spin")     == 0) wz =  0.5f;
    mecanum.setVelocity(vx, vy, wz);
}

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(4000);
    while (!Serial && (millis() < 2000)) delay(10);
    Serial.println("\n=== ROBO-OMNI boot ===");

    // 1. LEDs — feedback immédiat dès le démarrage
    leds.begin();
    leds.setMode(LedMode::IDLE);
    leds.update();

    // 2. Moteurs via PCA9685 (I2C 0x40)
    // ── Bus I2C — configuré ICI UNE SEULE FOIS, pins passés aux modules ────────
    Wire.begin(5, 6);      // SDA=GPIO5, SCL=GPIO6  (XIAO ESP32S3 D4/D5)
    Wire.setClock(50000);  // 50 kHz pour debug (passer à 400kHz en prod)
    // ─────────────────────────────────────────────────────────────────────────

    i2c_scan();           // scan complet avant toute init module
    delay(50);            // laisser le bus I2C se stabiliser après le scan

    /*if (!mecanum.begin(Wire)) {
        Serial.println("[ERREUR] PCA9685 absent — arrêt");
        leds.setMode(LedMode::ERROR);
        while (1) { leds.update(); delay(100); }
    }
    Serial.println("[OK] MecanumDrive");
*/
    // 3. ToF VL53L1X — Wire injecté depuis main
    hw_tof = tof.begin(Wire);
    if (!hw_tof) Serial.println("[WARN] TOF absent, évitement désactivé");
    else         Serial.println("[OK] TofSensor");

    // 4. IMU MPU-6050 — Wire injecté depuis main
    hw_imu = imu.begin(Wire);
    if (!hw_imu) Serial.println("[WARN] IMU absent, angles désactivés");
    else         Serial.println("[OK] ImuSensor");

    // 5. Serveur web
    // Coordonnées polaires : jx = vx (latéral), jy = vy (avant/arrière)
    web.setCommandCallback([](float vx, float vy, float wz) {
        mecanum.setVelocityPolar(vx, vy, wz);
    });
    if (!web.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("[WARN] WiFi non connecté, serveur web désactivé");
    } else {
        Serial.println("[OK] WebServer");
    }

    // 6. Voix PDM (décommenter quand Edge Impulse importé)
    /*
    voice.setCommandCallback(voiceCb);
    hw_voice = voice.begin();
    if (hw_voice) voice.startListening();
    */

    // 7. Caméra OV2640 (décommenter quand testée)
    /*
    hw_camera = tracker.begin();
    if (hw_camera) tracker.setTargetColor(255, 0, 0, 40);
    */

    Serial.println("=== ROBO-OMNI prêt ===");
    leds.setMode(LedMode::IDLE);
}

// ─── loop() ──────────────────────────────────────────────────────────────────
static uint32_t last_sensor_send = 0;

void loop() {
    // Évitement obstacle (TOF branché)
    if (hw_tof) {
        uint16_t dist;
        tof.readDistance(&dist);
        if (dist < 200) {
            leds.setMode(LedMode::OBSTACLE);
            mecanum.stop();
        }
    }

    // Mise à jour IMU
    if (hw_imu) {
        imu.update();
    }

    // Suivi objet par caméra
    if (hw_camera && tracker.isTracking()) {
        leds.setMode(LedMode::TRACKING);
        float vx = 0.0f, vy = 0.0f;
        tracker.track(&vx, &vy);
        mecanum.setVelocity(vx, vy, 0.0f);
    }

    // Télémétrie web toutes les 500ms
    if (millis() - last_sensor_send > 500) {
        uint16_t f = hw_tof ? 9999 : 0;  // 0 si TOF absent (pour éviter les valeurs invalides)
        float roll = hw_imu ? imu.getRoll() : 0.0f;
        float pitch = hw_imu ? imu.getPitch() : 0.0f;
        if (hw_tof) tof.readDistance(&f);
        web.sendSensorData(f, 0, 0, roll, pitch);
        last_sensor_send = millis();
    }

    // Animation LEDs
    leds.update();
}
