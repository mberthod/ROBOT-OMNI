/**
 * @file mecanum_drive.cpp
 * @brief Implémentation du driver Mecanum via PCA9685 + MX1508
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "mecanum_drive.h"
#include <Wire.h>

// ─── Correspondance canaux PCA9685 ───────────────────────────────────────────
// MX1508 : avancer = IN1(fwd)=PWM, IN2(rev)=0
//          reculer = IN1(fwd)=0,   IN2(rev)=PWM
//          stop    = IN1=0,         IN2=0
//
// Canal | Broche MX1508 | Moteur
//   0   | IN1           | FL (avant)
//   1   | IN2           | FL (arrière)
//   2   | IN3           | RL (avant)
//   3   | IN4           | RL (arrière)
//   4   | IN4           | FR (arrière) ← câblage inversé
//   5   | IN3           | FR (avant)
//   6   | IN2           | RR (arrière) ← câblage inversé
//   7   | IN1           | RR (avant)

static constexpr uint8_t CH_FL_FWD = 0;
static constexpr uint8_t CH_FL_REV = 1;
static constexpr uint8_t CH_RL_FWD = 2;
static constexpr uint8_t CH_RL_REV = 3;
static constexpr uint8_t CH_FR_FWD = 4;  // IN3 = avant (physiquement inversé)
static constexpr uint8_t CH_FR_REV = 5;  // IN4 = arrière
static constexpr uint8_t CH_RR_FWD = 6;  // IN1 = avant (physiquement inversé)
static constexpr uint8_t CH_RR_REV = 7;  // IN2 = arrière

/**
 * @brief Contrôle un moteur MX1508 via deux canaux PCA9685
 * @param pwm  Driver PCA9685
 * @param fwd  Canal IN1 (sens avant)
 * @param rev  Canal IN2 (sens arrière)
 * @param spd  Vitesse [-1.0, +1.0]
 */
static void setMotor(Adafruit_PWMServoDriver& pwm,
                     uint8_t fwd, uint8_t rev, float spd) {
    constexpr uint16_t MAX = 4095;
    uint16_t duty = static_cast<uint16_t>(fabsf(spd) * MAX);
    duty = min(duty, MAX);

    if (spd > 0.01f) {
        pwm.setPWM(fwd, 0, duty);
        pwm.setPWM(rev, 0, 0);
    } else if (spd < -0.01f) {
        pwm.setPWM(fwd, 0, 0);
        pwm.setPWM(rev, 0, duty);
    } else {
        pwm.setPWM(fwd, 0, 0);
        pwm.setPWM(rev, 0, 0);
    }
}

/**
 * @brief Initialise le PCA9685 à 1000 Hz
 * @return true si PCA9685 répond
 */
bool MecanumDrive::begin() {
    // ── Software Reset PCA9685 ────────────────────────────────────────────────
    // Nécessaire pour effacer EXTCLK et SLEEP bits si chip dans état corrompu.
    // SWRST : adresse générale 0x00, commande 0x06
    Wire.beginTransmission(0x00);
    Wire.write(0x06);
    Wire.endTransmission();
    delay(10);
    Serial.println("[PCA] SWRST envoyé");

    if (!pwm.begin()) {
        Serial.printf("[ERREUR] PCA9685 non trouvé à 0x%02X\n", PCA9685_ADDR);
        return false;
    }
    pwm.setOscillatorFrequency(25000000);  // oscillateur interne PCA9685 = 25 MHz
    pwm.setPWMFreq(PWM_FREQ_HZ);
    stop();
    Serial.printf("[OK] PCA9685 @0x%02X — %u Hz\n", PCA9685_ADDR, PWM_FREQ_HZ);
    return true;
}

/**
 * @brief Calcule et applique la cinématique Mecanum
 * @param vx Avant/arrière [-1.0, +1.0]
 * @param vy Gauche/droite [-1.0, +1.0]
 * @param wz Rotation       [-1.0, +1.0]
 */
void MecanumDrive::setVelocity(float vx, float vy, float wz) {
    // Cinématique 4 roues Mecanum
    float fl =  vx - vy - wz;
    float fr =  vx + vy + wz;
    float rl =  vx + vy - wz;
    float rr =  vx - vy + wz;

    // Normalisation pour ne jamais dépasser [-1, +1]
    float peak = max({fabsf(fl), fabsf(fr), fabsf(rl), fabsf(rr), 1.0f});
    fl /= peak;
    fr /= peak;
    rl /= peak;
    rr /= peak;

    setMotor(pwm, CH_FL_FWD, CH_FL_REV, fl);
    setMotor(pwm, CH_RL_FWD, CH_RL_REV, rl);
    setMotor(pwm, CH_FR_FWD, CH_FR_REV, fr);
    setMotor(pwm, CH_RR_FWD, CH_RR_REV, rr);
}

/**
 * @brief Cinématique Mecanum par coordonnées polaires (vecteurs à 45°)
 * @param jx Composante latérale joystick  [-1.0, +1.0]
 * @param jy Composante avant/arrière      [-1.0, +1.0]
 * @param wz Rotation                      [-1.0, +1.0]
 */
void MecanumDrive::setVelocityPolar(float jx, float jy, float wz) {
    // ── Étape 1 : conversion polaire ─────────────────────────────────────────
    // atan2 couvre 360° en tenant compte du signe de x et y
    float theta = atan2f(jy, jx);
    float force = sqrtf(jx * jx + jy * jy);
    if (force > 1.0f) force = 1.0f;  // clamp à l'unité

    // ── Étape 2 : décalage de phase -π/4 ─────────────────────────────────────
    // Aligne le référentiel sur les diagonales des roues Mecanum
    float p1 = sinf(theta - M_PI_4) * force;  // FR et RL
    float p2 = cosf(theta - M_PI_4) * force;  // FL et RR

    // ── Étape 3 : attribution moteurs ────────────────────────────────────────
    float fl = p2;
    float fr = p1;
    float rl = p1;
    float rr = p2;

    // ── Étape 4 : superposition rotation ─────────────────────────────────────
    // wz > 0 = horaire : gauche recule, droite avance
    fl -= wz;
    fr += wz;
    rl -= wz;
    rr += wz;

    // ── Étape 5 : normalisation (scaling) ────────────────────────────────────
    // Si un moteur dépasse 1.0, ramener tout à l'échelle sans changer le cap
    float peak = fabsf(fl);
    if (fabsf(fr) > peak) peak = fabsf(fr);
    if (fabsf(rl) > peak) peak = fabsf(rl);
    if (fabsf(rr) > peak) peak = fabsf(rr);
    if (peak > 1.0f) {
        fl /= peak;
        fr /= peak;
        rl /= peak;
        rr /= peak;
    }

    setMotor(pwm, CH_FL_FWD, CH_FL_REV, fl);
    setMotor(pwm, CH_RL_FWD, CH_RL_REV, rl);
    setMotor(pwm, CH_FR_FWD, CH_FR_REV, fr);
    setMotor(pwm, CH_RR_FWD, CH_RR_REV, rr);
}

/**
 * @brief Arrête tous les moteurs (roue libre)
 */
void MecanumDrive::stop() {
    for (uint8_t ch = 0; ch < 8; ch++) {
        pwm.setPWM(ch, 0, 0);
    }
}
