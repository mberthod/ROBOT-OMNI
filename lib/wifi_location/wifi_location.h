/**
 * @file wifi_location.h
 * @brief Géolocalisation par RSSI WiFi via weighted centroid
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <vector>

/**
 * @brief Point de scan WiFi (résultat brut)
 */
struct ScannedAP {
    String ssid;
    int8_t rssi;
};

/**
 * @brief Anchor WiFi pour triangulation RSSI
 */
struct WifiAnchor {
    String ssid;
    float x;
    float y;
    int8_t rssi_ref;  // RSSI à 1m
    float path_loss;  // Exposant path loss (n)
};

/**
 * @class WifiLocation
 * @brief Géolocalisation par RSSI via weighted centroid
 */
class WifiLocation {
public:
    /**
     * @brief Initialise le scan WiFi
     */
    void begin();

    /**
     * @brief Scanne les AP WiFi disponibles
     * @return Nombre d'AP trouvés
     */
    int8_t scan();

    /**
     * @brief Calcule la position par weighted centroid
     * @param[out] x Coordonnée X (si possible)
     * @param[out] y Coordonnée Y (si possible)
     * @return true si position calculable
     */
    bool getPosition(float* x, float* y);

    /**
     * @brief Ajoute une ancre de calibration
     * @param ssid SSID de l'ancre
     * @param x Coordonnée X
     * @param y Coordonnée Y
     * @param rssi_ref RSSI à 1m
     * @param path_loss Exposant path loss (n defaults to 2.0)
     */
    void addAnchor(const char* ssid, float x, float y, int8_t rssi_ref, float path_loss = 2.0f);

    /**
     * @brief Efface toutes les ancres
     */
    void clearAnchors();

    /**
     * @brief Obtient le meilleur AP (RSSI max)
     * @param[out] ssid SSID du meilleur AP
     * @param[out] rssi RSSI du meilleur AP
     * @return true si AP disponible
     */
    bool getBestAP(String* ssid, int8_t* rssi);

private:
    std::vector<WifiAnchor> anchors;
    std::vector<ScannedAP> last_scan;

    /**
     * @brief Calcule distance estimée via modèle log-distance
     * @param rssi RSSI mesuré
     * @param anchor Ancre de référence
     * @return Distance estimée (m)
     */
    float calculateDistance(int8_t rssi, const WifiAnchor& anchor);
};
