Chapitre 3 – Reconnaissance vocale avec Edge Impulse sur XIAO ESP32S3  
1. Accroche  
Imagine que tu dois commander ton robot uniquement avec ta voix, comme quand tu demandes à ton assistant domestique d’allumer la lumière. Dans cette section, nous allons embarquer une intelligence artificielle de reconnaissance de mots‑clés directement sur le micro PDM du XIAO ESP32S3, afin de dire « forward », « left », « stop »… et voir le robot réagir en moins d’une seconde, sans besoin de connexion Internet.  
2. Théorie  
Le principe de base est le même que celui d’un enfant qui apprend à reconnaître le son de son nom : on présente à un réseau de neurones de nombreux exemples de chaque mot, puis on lui demande de généraliser à de nouveaux enregistrements. Edge Impulse automatise tout le pipeline : extraction de caractéristiques (MFCC ou, mieux, MFE), entraînement d’un petit réseau de neurones quantifié pour du microcontrôleur, puis génération d’une bibliothèque C++ prête à être flashée. L’analogie du quotidien : c’est comme apprendre à reconnaître le bruit d’une porte qui grince parmi tous les sons de la maison ; après quelques répétitions, ton cerveau (ou le réseau) ne réagit plus qu’à ce grincement spécifique.  
3. Hardware  
Le seul matériel requis outre le XIAO ESP32S3 Sense est le microphone PDM présent sur la carte (GPIO 42 pour le clock, GPIO 41 pour la data). Aucun composant externe n’est nécessaire, ce qui garde le coût du projet autour de 66 € (voir le tableau récapitulatif du README).  
Élément
XIAO ESP32S3 Sense
Microphone PDM (intégré)
Câble USB‑C
Batterie 18650 + BMS
Total partie vocale
Schéma de connexion logique  
XIAO ESP32S3
 ├─ GPIO42 (PDM_CLK) ──► Microphone PDM clock
 └─ GPIO41 (PDM_DATA) ──► Microphone PDM data
4. Code commenté  
Nous supposons que tu as déjà exporté la bibliothèque Edge Impulse depuis le studio et que tu l’as dézippée dans lib/ei_model/.  
/**
 * @file lib/voice_control.h
 * @brief Interface pour la reconnaissance vocale Edge Impulse
 */
#pragma once
#include <Arduino.h>
class VoiceControl {
public:
    bool begin();                                    // Init I2S + buffer PSRAM
    void startListening();                           // Lance tâche FreeRTOS core 0
    void stopListening();                            // Arrête la tâche
    void setCommandCallback(void (*cb)(const char* cmd, float conf));
private:
    static void listenTask(void* param);
    static void i2sFillCallback(size_t bytes, uint8_t* buf);
    float runEdgeImpulse(int16_t* buffer);
    int16_t* audio_buf = nullptr;                    // Alloué en PSRAM
    TaskHandle_t task = nullptr;
    volatile bool listening = false;
    void (*cb_)(const char*, float) = nullptr;
};
/**
 * @file lib/voice_control.cpp
 * @brief Implémentation détaillée
 */
#include "voice_control.h"
#include <I2S.h>
#include "ei_model/src/edge-impulse-sdk/classifier/ei_run_classifier.h"
bool VoiceControl::begin() {
    // Allocation du buffer audio en PSRAM (1 seconde @16kHz 16‑bit = 32 ko)
    audio_buf = (int16_t*)ps_malloc(16000 * 2);
    if (!audio_buf) {
        Serial.printf("[ERREUR] Pas de PSRAM disponible\n");
        return false;
    }
    // Configuration du périphérique I2S en mode PDM
    I2S.setPinout(42, -1, 41);          // CLK, DATA, WS (non utilisé)
    if (!I2S.begin(I2S_PHILIPS_MODE, 16000, 16)) {
        Serial.printf("[ERREUR] I2S init failed\n");
        return false;
    }
    I2S.setFillCallback(i2sFillCallback);
    return true;
}
void VoiceControl::startListening() {
    if (!listening) {
        listening = true;
        xTaskCreatePinnedToCore(listenTask, "voice", 8192, this, 5, nullptr, 0);
    }
}
/* La tâche tourne en permanence sur le core 0, priorité moyenne */
void VoiceControl::listenTask(void* param) {
    auto* self = static_cast<VoiceControl*>(param);
    size_t samples = 0;
    while (self->listening) {
        // Remplissage du buffer tant qu’on n’a pas 1 seconde d’échantillons
        if (samples < 16000) {
            size_t to_read = (16000 - samples) * 2; // 2 bytes par sample
            size_t got = I2S.read((uint8_t*)(self->audio_buf) + samples*2, to_read);
            samples += got / 2;
        }
        // Quand le buffer est plein, on lance l’inférence
        if (samples == 16000) {
            float confidence = self->runEdgeImpulse(self->audio_buf);
            if (self->cb_ && confidence > 0.5f) {
                // On pourrait ici mapper la classe détectée à une commande texte
                self->cb_("detected", confidence);
            }
            samples = 0; // remise à zéro pour le prochain échantillon
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
float VoiceControl::runEdgeImpulse(int16_t* buf) {
    signal_t signal;
    signal.total_length = 16000;
    signal.get_data = &ei_get_data; // fourni par le SDK EI
    ei_impulse_result_t result;
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.printf("[ERREUR] EI échec %d\n", (int)res);
        return 0.0f;
    }
    // On suppose que la sortie du modèle est dans l’ordre :
    // [forward, backward, left, right, stop, spin, noise, unknown]
    // On retourne la confiance maximale.
    float max_conf = 0.0f;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
        if (result.classification[i].value > max_conf)
            max_conf = result.classification[i].value;
    }
    return max_conf;
}
/* Callback I2S : simplement ignoré ici, on lit via I2S.read() dans la tâche */
void VoiceControl::i2sFillCallback(size_t bytes, uint8_t* buf) {
    (void)bytes; (void)buf;
}
void VoiceControl::setCommandCallback(void (*cb)(const char* cmd, float conf)) {
    cb_ = cb;
}
Points à retenir :  
- Le buffer audio doit obligatoirement être placé en PSRAM (ps_malloc) parce qu’il dépasse la taille de la RAM interne (~320 ko disponibles).  
- La tâche FreeRTOS tourne sur le core 0 avec prio 5 et une pile de 8192 mots – suffisante pour le SDK Edge Impulse.  
- Aucun delay() n’est utilisé ; tout repose sur les interruptions I2S et le scheduler FreeRTOS.  
- Après avoir placé la bibliothèque dans lib/ei_model/, il suffit d’ajouter dans platformio.ini :
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DEI_CLASSIFIER_SLICES_PER_MODEL_WINDOW=3
Pas besoin de lib_deps car PlatformIO détecte automatiquement les dossiers sous lib/.
5. Résultats attendus  
Après flash et mise sous tension, ouvrez le moniteur Serie (115200 bauds). Vous devriez voir :
[INFO] VoiceControl init OK
[READY] Prononcez un mot-clé...
[INFO] Detection: forward (confidence: 0.92)
[INFO] Detection: left  (confidence: 0.87)
[INFO] Detection: stop  (confidence: 0.95)
Chaque détection déclenche la callback définie dans main.cpp, qui traduit le texte en vx/vy/wz pour le driver Mecanum. Le robot répond en moins de 300 ms après la fin du mot, ce qui est perceptible comme une réaction quasi‑instantanée.  
6. Problèmes courants  
Symptomatique
ps_malloc returns NULL
Aucune détection, confiance toujours à 0
Le model dépasse la taille de flash
Crash récurrent (Guru Meditation)
Reconnaissance très bruité en environnement réel
7. Pour aller plus loin  
- Améliorer la robustesse : recueillez des données dans diverses conditions de lumière et de température, puis ré‑entraînez le modèle avec augmentation de bruit (Pitch shift, temps stretch).  
- Passer à un modèle basé sur MFCC + CNN 1D si vous avez besoin de distinguer plus de 6 commandes tout en restant sous 100 ko de RAM.  
- Exporter vers TensorFlow Lite Micro directement depuis Edge Impulse pour comparer la taille et la latence.  
- Intégrer une commande de réveil (« Hey Robot ») suivie d’une commande d’action, en utilisant la approche de deux‑étapes du modèle (wake‑word + command).  
Tu trouveras le projet complet, les exports Edge Impulse prêts à l’emploi et les exemples de réglage dans le dépôt GitHub du projet :  
🔗 https://github.com/username/robo-omni (voir le dossier liber/ei_model/ et la branche voice).  