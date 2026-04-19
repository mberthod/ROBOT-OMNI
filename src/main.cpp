/**
 * @file main.cpp
 * @brief Fichier principal ROBO-OMNI : contrôle global et boucle principale
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include <Arduino.h>
#include "mecanum_drive.h"
#include "tof_sensor.h"  // Changé pour utiliser un seul capteur
#include "imu_sensor.h"
#include "status_leds.h"
#include "web_server.h"
#include "wifi_location.h"
#include "voice_control.h"
#include "object_tracker.h"

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
static TofSensors    tof;
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
    Wire.begin(5, 6);  // SDA=GPIO5, SCL=GPIO6 XIAO ESP32S3

    // ── Scan I2C pour confirmer les périphériques présents ──────────────────
    Serial.println("[I2C] Scan du bus...");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Périphérique trouvé à 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) Serial.println("[I2C] AUCUN périphérique trouvé — vérifier câblage SDA/SCL");
    // ────────────────────────────────────────────────────────────────────────

    if (!mecanum.begin()) {
        Serial.println("[ERREUR] PCA9685 absent — arrêt");
        leds.setMode(LedMode::ERROR);
        while (1) { leds.update(); delay(100); }
    }
    Serial.println("[OK] MecanumDrive");

    // 3. ToF VL53L1X (décommenter quand branché)
    /*
    hw_tof = tof.begin();
    if (!hw_tof) Serial.println("[WARN] TOF absents, évitement désactivé");
    else         Serial.println("[OK] TofSensors");
    */

    // 4. IMU MPU-6050 (décommenter quand branché)
    /*
    hw_imu = imu.begin();
    if (!hw_imu) Serial.println("[WARN] IMU absent, angles désactivés");
    else         Serial.println("[OK] ImuSensor");
    */

    // 5. Serveur web
    // Coordonnées polaires : jx = composante latérale (vy), jy = avant/arrière (vx)
    web.setCommandCallback([](float vx, float vy, float wz) {
        mecanum.setVelocityPolar(vy, vx, wz);
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
    // Évitement obstacle (seulement si TOF branché)
    if (hw_tof) {
        uint16_t front, left, right;
        if (tof.readAll(&front, &left, &right)) {
            if (front < 200 || left < 200 || right < 200) {
                leds.setMode(LedMode::OBSTACLE);
                mecanum.stop();
            }
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
        uint16_t f = 9999, l = 9999, r = 9999;
        float roll = 0.0f, pitch = 0.0f;
        if (hw_tof) tof.readAll(&f, &l, &r);
        if (hw_imu) { roll = imu.getRoll(); pitch = imu.getPitch(); }
        web.sendSensorData(f, l, r, roll, pitch);
        last_sensor_send = millis();
    }

    // Animation LEDs
    leds.update();
}
