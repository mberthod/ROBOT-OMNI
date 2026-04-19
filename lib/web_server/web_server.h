/**
 * @file web_server.h
 * @brief Driver du serveur web pour contrôle et télémétrie du robot
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

/**
 * @class WebServer
 * @brief Serveur web async avec joystick HTML et télémétrie JSON
 */
class WebServer {
public:
    /// AsyncWebServer requiert un port dans son constructeur
    WebServer() : server(80) {}

    /**
     * @brief Connecte au WiFi et démarre le serveur HTTP + mDNS
     * @param ssid SSID WiFi
     * @param password Mot de passe WiFi
     * @return true si connexion établie
     */
    bool begin(const char* ssid, const char* password);

    /**
     * @brief Définit la callback pour les commandes de mouvement
     * @param cb Callback(vx, vy, wz) en [-1.0, +1.0]
     */
    void setCommandCallback(void (*cb)(float vx, float vy, float wz));

    /**
     * @brief Met à jour le cache des données capteurs (lu par GET /sensors)
     */
    void sendSensorData(uint16_t tof_f, uint16_t tof_l, uint16_t tof_r,
                        float roll, float pitch);

private:
    AsyncWebServer server;
    void (*command_cb)(float, float, float) = nullptr;

    // Cache capteurs pour GET /sensors
    uint16_t cache_tof_f  = 9999;
    uint16_t cache_tof_l  = 9999;
    uint16_t cache_tof_r  = 9999;
    float    cache_roll   = 0.0f;
    float    cache_pitch  = 0.0f;

    /// Constantes
    static constexpr uint16_t WEB_PORT = 80;

    String buildMainPage();
    String toSensorJson();
};
