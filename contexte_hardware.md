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
- Microphone : MSM261D3526H1CPM (PDM/I2S) — INCLUS sur carte Sense
- MicroSD : slot sur carte Sense (SPI, max 32GB FAT32)
- WiFi : 802.11b/g/n 2.4GHz
- Bluetooth : BLE 5.0
- Antenne : U.FL externe (incluse dans le kit)
- Dimensions : 21 × 17.5 mm
- LED builtin : GPIO21 (INVERSÉE — LOW = allumée)
- Bootloader : tenir BOOT + connecter USB

## Pinout XIAO ESP32S3 Sense — 11 GPIO utilisables
```
Pin label | GPIO  | Fonctions possibles        | Usage robot OMNI
----------|-------|----------------------------|------------------
D0 / A0   | GPIO1 | ADC, GPIO, Touch           | L298N_A_IN1
D1 / A1   | GPIO2 | ADC, GPIO, Touch           | L298N_A_IN2
D2 / A2   | GPIO3 | ADC, GPIO, Touch           | L298N_B_IN1
D3 / A3   | GPIO4 | ADC, GPIO, Touch, SPI_CS   | L298N_B_IN2
D4 / SDA  | GPIO5 | I2C SDA                    | I2C SDA (IMU + TOF)
D5 / SCL  | GPIO6 | I2C SCL                    | I2C SCL (IMU + TOF)
D6 / TX   | GPIO43| UART0 TX                   | UART debug / TX
D7 / RX   | GPIO44| UART0 RX                   | UART debug / RX
D8 / SCK  | GPIO7 | SPI CLK                    | L298N_C_IN1
D9 / MISO | GPIO8 | SPI MISO, I2S              | L298N_C_IN2
D10/ MOSI | GPIO9 | SPI MOSI                   | L298N_D_IN1
```
⚠️ GPIO41/42 = pins supplémentaires sur Sense (connecteur B2B) → L298N_D_IN2
⚠️ GPIO21 = LED builtin seulement
⚠️ Seulement 9 ADC disponibles (pas GPIO41/42)
⚠️ GPIO1-10 safe pour GPIO général

## Alimentation
- Source : LiPo 2S (7.4V nominal, 8.4V max chargé)
- Conversion : buck converter 7.4V → 5V (ex: MP2315 ou LM2596)
- XIAO alimenté : via pin 5V (avec diode Schottky si USB simultané)
- L298N alimenté : 5V logique + 7.4V moteurs directement depuis LiPo 2S
- Moteurs : 4× TT-motor (3-6V nominaux) → alimentés via L298N depuis 5V conv.
- 3.3V capteurs : depuis régulateur interne XIAO (700mA max — attention budget)

## Actionneurs — Moteurs
- 4× TT-motor jaune plastique (DC, sans encodeur)
- 2× pont en H L298N
  - L298N_A : moteur avant-gauche (FL) + moteur arrière-gauche (RL)
  - L298N_B : moteur avant-droit (FR) + moteur arrière-droit (RR)
- Contrôle : PWM vitesse via LEDC ESP32 + GPIO direction (IN1/IN2/IN3/IN4)
- Roues : omnidirectionnelles Mecanum 48mm ou 60mm

## Cinématique Mecanum (4 roues)
```
Légende : FL=avant-gauche, FR=avant-droit, RL=arrière-gauche, RR=arrière-droit
Vitesse roue = f(Vx, Vy, Wz) :
  FL =  Vx - Vy - Wz
  FR =  Vx + Vy + Wz
  RL =  Vx + Vy - Wz
  RR =  Vx - Vy + Wz
Vx = avance/recul, Vy = déplacement latéral, Wz = rotation
```

## Capteurs
### Caméra OV2640 (intégrée Sense)
- Résolution max : 1600×1200 (UXGA)
- Interface : connecteur B2B dédié (pins caméra réservées, pas GPIO user)
- Usage : suivi d'objet, streaming WiFi, capture image

### Microphone PDM (intégré Sense)
- Modèle : MSM261D3526H1CPM
- Interface : I2S/PDM (pins dédiées, pas GPIO user)
- Usage : reconnaissance vocale, wake word

### 3× TOF VL53L1X (détection obstacles)
- Interface : I2C (adresse 0x29 par défaut — PROBLÈME si 3 sur même bus)
- Solution adressage : XSHUT pin par capteur pour reset séquentiel + adresse unique
  - TOF_1 XSHUT → GPIO libre (ex: via PCA9555 expander)
  - TOF_2 XSHUT → GPIO libre
  - TOF_3 XSHUT → GPIO libre
- Portée : 4cm à 4m selon mode
- Positionnement : avant / avant-gauche / avant-droit

### IMU (à définir)
- Recommandé : MPU-6050 (I2C, adresse 0x68/0x69, <1€)
  ou LSM6DS3 (I2C, plus précis, ~3€)
- Interface : I2C partagé avec TOF (même bus SDA/SCL)
- Usage : détection orientation, compensation dérive

## Évolutions prévues
### LEDs adressables
- Type : WS2812B ou SK6812 (1 fil data)
- Disposition : anneau ou bande autour du châssis
- Contrôle : 1 GPIO (bibliothèque FastLED ou NeoPixel)
- Alimentation : 5V dédié (chaque LED = 60mA max — prévoir alim séparée)

### Géolocalisation WiFi RSS
- Principe : scan des réseaux WiFi visibles + RSSI → triangulation
- API : WiFi.scanNetworks() + calcul position par fingerprinting
- Précision : 2-5m en intérieur

## Serveur web de contrôle
- Stack : ESP-IDF + esp_http_server
- Interface : HTML/JS minimaliste, joystick virtuel
- Fonctions : contrôle Vx/Vy/Wz, affichage flux caméra, état capteurs
- Port : 80 (HTTP)

## Budget estimé
| Composant | Prix indicatif |
|-----------|----------------|
| XIAO ESP32S3 Sense | ~14€ |
| 4× TT-motor Mecanum | ~8€ |
| 2× L298N module | ~4€ |
| Châssis acrylique 4 roues | ~12€ |
| 3× VL53L1X module | ~12€ |
| MPU-6050 module | ~1€ |
| Buck converter 2S→5V | ~2€ |
| LiPo 2S 1500mAh | ~10€ |
| Câbles + divers | ~3€ |
| **TOTAL** | **~66€** |

## Références GitHub et documentation
- Repo exemples XIAO S3 Sense : https://github.com/Mjrovai/XIAO-ESP32S3-Sense
- Wiki Seeed : https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
- Pin multiplexing : https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/
- Framework : Arduino ESP32 (plus accessible que ESP-IDF pour cette série)
- Build : PlatformIO CLI

## Contraintes temps réel
- Boucle moteurs : 50 Hz (20ms) — TT-motor, pas besoin plus rapide
- Lecture TOF : 10 Hz (100ms) par capteur en séquentiel
- Inférence IA caméra : 5-10 Hz selon modèle
- Wake word audio : continu, tâche Core 0 dédiée
- Serveur web : event-driven, Core 1

## Allocation cores FreeRTOS
- Core 0 : audio (wake word), WiFi stack
- Core 1 : moteurs, capteurs, IA caméra, serveur web
