/**
 * @file wifi_location.cpp
 * @brief Implémentation du positionnement WiFi par RSSI.
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "wifi_location.h"
#include <WiFi.h>
#include <cmath>

/**
 * @brief Initialise le scan WiFi
 */
void WifiLocation::begin() {
    WiFi.mode(WIFI_STA);
    // Pas de connexion needed pour scan
}

/**
 * @brief Scanne les AP WiFi disponibles
 * @return Nombre d'AP trouvés
 */
int8_t WifiLocation::scan() {
    int16_t n = WiFi.scanNetworks(false, true, false);

    last_scan.clear();
    for (int16_t i = 0; i < n; i++) {
        ScannedAP ap;
        ap.ssid = WiFi.SSID(i);
        ap.rssi = (int8_t)WiFi.RSSI(i);
        last_scan.push_back(ap);
    }

    return static_cast<int8_t>(n);
}

/**
 * @brief Calcule la position par weighted centroid
 * @param[out] x Coordonnée X (si possible)
 * @param[out] y Coordonnée Y (si possible)
 * @return true si position calculable
 */
bool WifiLocation::getPosition(float* x, float* y) {
    if (anchors.empty() || last_scan.empty()) {
        return false;
    }

    float sum_w = 0.0f;
    float sum_wx = 0.0f;
    float sum_wy = 0.0f;

    for (const auto& anchor : anchors) {
        // Trouver le RSSI pour cette ancre
        int8_t rssi = 0;
        for (const auto& ap : last_scan) {
            if (ap.ssid == anchor.ssid) {
                rssi = ap.rssi;
                break;
            }
        }

        if (rssi == 0) continue;  // Not found

        float dist = calculateDistance(rssi, anchor);
        float w = 1.0f / (dist * dist + 0.001f);  // Éviter div/0

        sum_w += w;
        sum_wx += w * anchor.x;
        sum_wy += w * anchor.y;
    }

    if (sum_w < 0.001f) {
        return false;  // Trop peu d'ancres valides
    }

    *x = sum_wx / sum_w;
    *y = sum_wy / sum_w;
    return true;
}

/**
 * @brief Ajoute une ancre de calibration
 * @param ssid SSID de l'ancre
 * @param x Coordonnée X
 * @param y Coordonnée Y
 * @param rssi_ref RSSI à 1m
 * @param path_loss Exposant path loss (n defaults to 2.0)
 */
void WifiLocation::addAnchor(const char* ssid, float x, float y, int8_t rssi_ref, float path_loss) {
    anchors.push_back({String(ssid), x, y, rssi_ref, path_loss});
}

/**
 * @brief Efface toutes les ancres
 */
void WifiLocation::clearAnchors() {
    anchors.clear();
}

/**
 * @brief Obtient le meilleur AP (RSSI max)
 * @param[out] ssid SSID du meilleur AP
 * @param[out] rssi RSSI du meilleur AP
 * @return true si AP disponible
 */
bool WifiLocation::getBestAP(String* ssid, int8_t* rssi) {
    if (last_scan.empty()) {
        return false;
    }

    int16_t max_idx = 0;
    int8_t max_rssi = -100;

    for (size_t i = 0; i < last_scan.size(); i++) {
        if (last_scan[i].rssi > max_rssi) {
            max_rssi = last_scan[i].rssi;
            max_idx = i;
        }
    }

    *ssid = last_scan[max_idx].ssid;
    *rssi = max_rssi;
    return true;
}

/**
 * @brief Calcule distance estimée via modèle log-distance
 * @param rssi RSSI mesuré
 * @param anchor Ancre de référence
 * @return Distance estimée (m)
 */
float WifiLocation::calculateDistance(int8_t rssi, const WifiAnchor& anchor) {
    float pl = anchor.rssi_ref - rssi;
    float dist = pow(10.0f, pl / (10.0f * anchor.path_loss));
    return dist;
}
