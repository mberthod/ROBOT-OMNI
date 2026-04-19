/**
 * @file web_server.h
 * @brief Serveur web async sur tâche FreeRTOS Core 0 avec reconnexion auto
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

/**
 * @class WebServer
 * @brief Serveur HTTP async — WiFi + mDNS + OTA sur tâche FreeRTOS Core 0
 * @note begin() est non-bloquant : retour immédiat, connexion en arrière-plan
 */
class WebServer {
public:
    WebServer() : server(80) {}

    /**
     * @brief Lance la tâche FreeRTOS WiFi+Web sur Core 0 (non-bloquant)
     * @param ssid     SSID WiFi
     * @param password Mot de passe WiFi
     * @param ip       IP statique (optionnel — INADDR_NONE = DHCP)
     * @param gateway  Passerelle routeur
     * @param subnet   Masque sous-réseau
     */
    void begin(const char* ssid, const char* password,
               IPAddress ip      = INADDR_NONE,
               IPAddress gateway = INADDR_NONE,
               IPAddress subnet  = IPAddress(255,255,255,0));

    /**
     * @brief Définit la callback commande de mouvement
     */
    void setCommandCallback(void (*cb)(float vx, float vy, float wz));

    /**
     * @brief Met à jour le cache capteurs (appelé depuis loop/tâche)
     * @note Thread-safe : copie atomique de scalaires
     */
    void sendSensorData(uint16_t tof_f, uint16_t tof_l, uint16_t tof_r,
                        float roll, float pitch);

    /**
     * @brief Retourne true si WiFi connecté et serveur actif
     */
    bool isReady() const { return _ready; }

private:
    AsyncWebServer server;

    void (*command_cb)(float, float, float) = nullptr;

    // Credentials + IP statique
    char      _ssid[64]    = {};
    char      _pass[64]    = {};
    IPAddress _static_ip   = INADDR_NONE;
    IPAddress _gateway     = INADDR_NONE;
    IPAddress _subnet      = IPAddress(255,255,255,0);

    // Cache capteurs
    volatile uint16_t cache_tof   = 9999;
    volatile float    cache_roll  = 0.0f;
    volatile float    cache_pitch = 0.0f;
    volatile bool     _ready      = false;

    TaskHandle_t task_handle = nullptr;
    static constexpr uint16_t WEB_PORT    = 80;
    static constexpr uint32_t STACK_SIZE  = 8192;

    /**
     * @brief Tâche FreeRTOS Core 0 : WiFi → routes → server.begin → reconnect
     */
    static void wifiTask(void* params);

    /**
     * @brief Configure toutes les routes HTTP (appelé une fois dans la tâche)
     */
    void setupRoutes();

    String buildMainPage();
    String toSensorJson();
};
