# Contexte Hardware — Robot Omnidirectionnel XIAO ESP32S3 Sense

## Identité du projet
- Nom de code : ROBO-OMNI
- Objectif : robot 4 roues omnidirectionnelles avec IA embarquée
- Cas d'usage IA : reconnaissance vocale + suivi d'objet par caméra
- Repo de référence : https://github.com/Mjrovai/XIAO-ESP32S3-Sense
- Wiki carte : https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

## MCU — XIAO ESP32S3 Sense
- Puce : ESP32-S3R8 (dual-core Xtensa LX7 @ 240 MHz)
- Flash : 8 MB
- PSRAM : 8 MB Octal (intégré sur puce)
- Caméra : OV2640 (1600×1200) sur connecteur B2B dédié — INCLUSE
- Microphone : MSM261D3526H1CPM (PDM/I2S) — INCLUS carte Sense
- MicroSD : slot sur carte Sense (SPI, max 32GB FAT32)
- WiFi : 802.11b/g/n 2.4GHz
- Bluetooth : BLE 5.0
- Antenne : U.FL externe incluse
- Dimensions : 21 × 17.5 mm
- LED builtin : GPIO21 (INVERSÉE — LOW=allumée, HIGH=éteinte)
- Bootloader : tenir BOOT + connecter USB

## Pinout XIAO ESP32S3 Sense — 11 GPIO utilisables
```
Pin label | GPIO  | Usage robot OMNI
----------|-------|------------------
D0 / A0   | GPIO1 | TOF_1_XSHUT
D1 / A1   | GPIO2 | TOF_2_XSHUT
D2 / A2   | GPIO3 | TOF_3_XSHUT
D3 / A3   | GPIO4 | WS2812B DATA (LEDs adressables)
D4 / SDA  | GPIO5 | I2C SDA (bus unique)
D5 / SCL  | GPIO6 | I2C SCL (bus unique)
D6 / TX   | GPIO43| UART TX debug
D7 / RX   | GPIO44| UART RX debug
D8        | GPIO7 | LIBRE
D9        | GPIO8 | LIBRE
D10       | GPIO9 | LIBRE
```
⚠️ GPIO21 = LED builtin uniquement
⚠️ Caméra sur connecteur B2B — pins non accessibles en GPIO
⚠️ Micro PDM sur carte Sense — pins non accessibles en GPIO
⚠️ GPIO41/42 = pins supplémentaires Sense (B2B), réservées si caméra montée

## Alimentation
- Source : LiPo 2S (7.4V nominal, 8.4V max chargé)
- Conversion : buck converter 2S → 5V (ex: MP2315 ou LM2596)
- XIAO : alimenté via pin 5V (avec diode Schottky si USB simultané)
- MX1508 : alimenté directement depuis buck 5V (moteurs TT 3-6V)
- PCA9685 VCC logique : 3.3V depuis XIAO
- PCA9685 V+ moteurs : 5V depuis buck (non utilisé — MX1508 gère)
- Capteurs I2C : 3.3V depuis XIAO (budget 700mA max)
- LEDs WS2812B : 5V depuis buck avec capa 1000µF en parallèle

## Bus I2C — UNIQUE sur SDA=GPIO5 / SCL=GPIO6
```
Adresse | Composant        | Notes
--------|------------------|-------------------------------
0x40    | PCA9685          | PWM moteurs (adresse défaut)
0x29    | VL53L1X TOF_1    | Adresse après init XSHUT séquentiel → 0x30
0x29    | VL53L1X TOF_2    | Adresse après init XSHUT séquentiel → 0x31
0x29    | VL53L1X TOF_3    | Adresse après init XSHUT séquentiel → 0x32
0x68    | MPU-6050 IMU     | AD0=GND → 0x68, AD0=3V3 → 0x69
```
⚠️ Procédure init TOF obligatoire :
  1. Mettre tous les XSHUT à LOW (désactiver tous les TOF)
  2. XSHUT_1 HIGH → configurer adresse 0x30
  3. XSHUT_2 HIGH → configurer adresse 0x31
  4. XSHUT_3 HIGH → configurer adresse 0x32

## Actionneurs — Moteurs

### Pont en H : MX1508 (2 canaux, mini module)
- ⚠️ PAS de pin Enable séparée — contrôle direct par PWM sur IN
- Tension moteurs : 1.8V à 6.5V (compatible TT-motor)
- Courant max : 1.5A par canal
- Contrôle par canal :
  ```
  Avancer : IN1=PWM(vitesse)  IN2=LOW
  Reculer : IN1=LOW           IN2=PWM(vitesse)
  Stop    : IN1=LOW           IN2=LOW
  Frein   : IN1=HIGH          IN2=HIGH  (ne pas utiliser en continu)
  ```
- Quantité : 2 modules MX1508 (4 canaux total = 4 moteurs)

### PCA9685 — 16 canaux PWM I2C
- Adresse I2C : 0x40 (défaut, A0-A5 = GND)
- Fréquence PWM : 1000 Hz pour moteurs DC
- Résolution : 12 bits (0-4095)
- Bibliothèque : Adafruit PCA9685
- Assignation canaux :
  ```
  CH0  → FL (avant-gauche)  IN1   (PWM vitesse)
  CH1  → FL (avant-gauche)  IN2   (PWM vitesse)
  CH2  → FR (avant-droit)   IN1
  CH3  → FR (avant-droit)   IN2
  CH4  → RL (arrière-gauche) IN1
  CH5  → RL (arrière-gauche) IN2
  CH6  → RR (arrière-droit)  IN1
  CH7  → RR (arrière-droit)  IN2
  CH8-CH15 → LIBRES (LEDs PWM ou servos futurs)
  ```

### Cinématique Mecanum (4 roues)
```
Vx = avance/recul [-1.0 à +1.0]
Vy = déplacement latéral [-1.0 à +1.0]
Wz = rotation [-1.0 à +1.0]

Vitesse par roue (normalisée) :
  FL =  Vx - Vy - Wz
  FR =  Vx + Vy + Wz
  RL =  Vx + Vy - Wz
  RR =  Vx - Vy + Wz

Conversion vitesse → PCA9685 (12 bits) :
  Si vitesse > 0 : CH_IN1 = vitesse * 4095, CH_IN2 = 0
  Si vitesse < 0 : CH_IN1 = 0, CH_IN2 = abs(vitesse) * 4095
  Si vitesse = 0 : CH_IN1 = 0, CH_IN2 = 0
```

## Capteurs

### 3× VL53L1X (ToF obstacles)
- Interface : I2C, adresse défaut 0x29
- XSHUT : GPIO1, GPIO2, GPIO3 (réassignation adresses au boot)
- Portée : 4cm à 4m
- Positionnement : avant-centre / avant-gauche / avant-droit
- Bibliothèque : VL53L1X (Pololu)

### IMU MPU-6050
- Interface : I2C, adresse 0x68
- Données : accéléromètre 3 axes + gyroscope 3 axes
- Bibliothèque : Adafruit MPU6050
- Usage : détection orientation, compensation dérive Mecanum

### Caméra OV2640 (intégrée Sense)
- Résolution : jusqu'à 1600×1200
- Interface : B2B connecteur (pins réservées, non GPIO user)
- Usage : suivi d'objet, streaming WiFi
- Bibliothèque : esp32-camera (incluse dans platform PlatformIO)
- Buffer frame : OBLIGATOIREMENT en PSRAM via ps_malloc()

### Microphone PDM (intégré Sense)
- Modèle : MSM261D3526H1CPM
- Interface : I2S/PDM (pins dédiées carte Sense)
- Usage : reconnaissance vocale, wake word
- Bibliothèque : I2S natif Arduino ESP32
- Buffer audio : OBLIGATOIREMENT en PSRAM via ps_malloc()

## Évolutions prévues

### LEDs adressables WS2812B
- Data : GPIO4 (D3)
- Alimentation : 5V buck avec 1000µF capa bulk
- Bibliothèque : FastLED
- ⚠️ Ne pas alimenter depuis 3.3V XIAO

### Géolocalisation WiFi RSS
- API : WiFi.scanNetworks() natif Arduino ESP32
- Principe : RSSI des AP visibles → fingerprinting position
- Précision : 2-5m intérieur

## Serveur web de contrôle
- Bibliothèque : ESPAsyncWebServer + AsyncTCP
- Port : 80
- Contenu : HTML/JS joystick virtuel, flux caméra MJPEG, état capteurs JSON
- ⚠️ Ne jamais utiliser delay() dans loop() avec AsyncWebServer

## IA embarquée
- Reconnaissance vocale : Edge Impulse (export lib Arduino → lib/)
- Suivi objet caméra : TFLite Micro ou Edge Impulse FOMO
- Tous les buffers modèles : PSRAM via ps_malloc()
- Tâche FreeRTOS dédiée Core 0 pour l'audio
- Inférence caméra : Core 1, non bloquante

## Allocation FreeRTOS
```
Core 0 : WiFi stack + audio wake word (tâche priorité 5)
Core 1 : moteurs (priorité 10) + capteurs (priorité 6) +
         inférence caméra (priorité 4) + serveur web (event-driven)
```

## Budget estimé
| Composant | Prix |
|-----------|------|
| XIAO ESP32S3 Sense | ~14€ |
| 4× TT-motor Mecanum | ~8€ |
| 2× MX1508 mini | ~3€ |
| PCA9685 module | ~2€ |
| Châssis acrylique 4 roues | ~12€ |
| 3× VL53L1X module | ~12€ |
| MPU-6050 module | ~1€ |
| Buck converter 2S→5V | ~2€ |
| LiPo 2S 1500mAh | ~10€ |
| Divers câbles | ~3€ |
| **TOTAL** | **~67€** |

## Bibliothèques PlatformIO
```
pololu/VL53L1X @ ^2.0.3
adafruit/Adafruit MPU6050 @ ^2.2.6
adafruit/Adafruit BusIO @ ^1.16.1
adafruit/Adafruit Unified Sensor @ ^1.1.14
adafruit/Adafruit PWM Servo Driver Library @ ^3.0.1
fastled/FastLED @ ^3.7.0
mathieucarbou/ESPAsyncWebServer @ ^3.3.23
mathieucarbou/AsyncTCP @ ^3.2.14
bblanchon/ArduinoJson @ ^7.2.1
```
