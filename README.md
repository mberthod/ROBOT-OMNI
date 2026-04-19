<!-- PROJECT SHIELDS -->

[![PlatformIO](https://img.shields.io/badge/PlatformIO-5c345b?style=flat-square&logo=espressif)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Arduino-00979D?style=flat-square&logo=arduino)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

<!-- ABOUT THE PROJECT -->

# ROBO-OMNI

Quadricycle omnidirectionnel avec reconnaissance vocale et suivi d'objet. Contrôlé via Web + commande vocale.

<img src="[IMAGE]" alt="ROBO-OMNI robot" width="500">

<!-- HARDWARE SPECS -->

## Hardware

| Composant            | Référence         | Prix    |
|----------------------|-------------------|---------|
| ESP32-S3 XIAO        | SEED_XIAO_ESP32S3 | 12€     |
| PCA9685              | NCP5665           | 1.5€    |
| MX1508 x4            | 2x                 | 2€      |
| VL53L1X x3           | VL53L1X           | 6.5€    |
| MPU-6050             | MPU6050           | 2€      |
| OV2640               | OV2640 + PCB      | 8€      |
| WS2812B x12          | 2020              | 2€      |
| Câbles + Batterie    | 18650 + BMS       | 5€      |
| -------------------- | ------------------ | ------ |
| **Total**            |                   | ~42€    |

<!-- CONNECTION DIAGRAM -->

## Câblage I2C

```
        ESP32-S3           PCA9685      Capteurs
    ┌──────────────┐     ┌────────────┐ ┌─────────────┐
    │              │     │            │ │              │
    │  SDA=GPIO5   ──────┤ SDA=48     ├─┤              │
    │    SCL=GPIO6  ──────┤ SCL=47     ├─┤              │
    │              │     │            │ │              │
    └──────────────┘     └────────────┘ └─────────────┘
                                      ├─ VL53L1x 1
                                      ├─ VL53L1x 2
                                      └─ VL53L1x 3
```

<!-- BUILD INSTRUCTIONS -->

## Build Commands

```bash
# 1. Installer PlatformIO
pip install platformio

# 2. Compiler (check)
platformio run -t check

# 3. Flash sur USB
platformio run -t upload
```

<!-- FEATURES -->

## Features

- ✅ **Mecanum drive** : Contrôle 4 roues PCA9685, cinématique complète [vx,vy,ωz]
- ✅ **Voice control** : Edge Impulse + micro PDM, commandes vocales "forward/left/stop/..."
- ✅ **Object tracking** : Caméra OV2640, détection couleur, PID simplifié, QVGA @30fps
- ✅ **Web server** : Joystick virtuel HTML, télémétrie temps réel WebSockets
- ✅ **WiFi location** : Triangulation RSSI, modèle log-distance, weighted centroid
- ✅ **Obstacle avoidance** : 3 capteurs ToF, arrêt automatique <200mm + LEDs clignotantes
- ✅ **LED feedback** : WS2812B, 6 modes (IDLE/MOVING/OBSTACLE/LISTENING/TRACKING/ERROR)

<!-- LINKS & REFERENCES -->

<!-- LINKS -->

### Ressources

- [XIAO ESP32S3 Wiki](https://wiki.seesaw.io/esp32s3/)
- [Robot Omni / Mjrovai](https://github.com/mjrovai/robot-omni)
- [Edge Impulse](https://edgeimpulse.com/)

<!-- LICENSE -->

### License

MIT License.
