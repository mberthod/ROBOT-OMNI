/**
 * @file web_server.cpp
 * @brief Serveur web sur tâche FreeRTOS Core 0 — WiFi + HTTP + mDNS + OTA
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "web_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>

// ─── API publique ──────────────────────────────────────────────────────────────

void WebServer::setCommandCallback(void (*cb)(float vx, float vy, float wz)) {
    command_cb = cb;
}

void WebServer::sendSensorData(uint16_t tof_f, uint16_t, uint16_t,
                                float roll, float pitch) {
    cache_tof   = tof_f;
    cache_roll  = roll;
    cache_pitch = pitch;
}

/**
 * @brief Lance la tâche WiFi+Web sur Core 0 — retour IMMÉDIAT (non-bloquant)
 */
void WebServer::begin(const char* ssid, const char* password,
                      IPAddress ip, IPAddress gateway, IPAddress subnet) {
    strncpy(_ssid, ssid,     sizeof(_ssid) - 1);
    strncpy(_pass, password, sizeof(_pass) - 1);
    _static_ip = ip;
    _gateway   = gateway;
    _subnet    = subnet;

    // Routes configurées une seule fois ici (avant le task, pas de race condition)
    setupRoutes();

    xTaskCreatePinnedToCore(
        wifiTask,
        "wifi_web",
        STACK_SIZE,
        this,
        2,               // priorité 2 — sous loop (1) mais stable
        &task_handle,
        0                // Core 0 — stack WiFi natif ESP32
    );

    Serial.println("[WEB] Tâche WiFi lancée sur Core 0");
}

// ─── Tâche FreeRTOS ────────────────────────────────────────────────────────────

/**
 * @brief Tâche Core 0 : connexion WiFi + démarrage serveur + reconnexion auto
 */
void WebServer::wifiTask(void* params) {
    auto* self = static_cast<WebServer*>(params);

    // ── Tentative de connexion avec retries ───────────────────────────────────
    auto connect = [&]() -> bool {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);

        // IP statique si configurée, sinon DHCP
        if ((uint32_t)self->_static_ip != 0) {
            if (WiFi.config(self->_static_ip, self->_gateway, self->_subnet)) {
                Serial.printf("[WIFI] IP statique : %s\n",
                              self->_static_ip.toString().c_str());
            }
        }

        WiFi.begin(self->_ssid, self->_pass);

        Serial.printf("[WIFI] Connexion à '%s'", self->_ssid);
        for (uint8_t i = 0; i < 40; i++) {    // 40 × 500ms = 20s max
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println(" OK");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }
        Serial.println(" TIMEOUT");
        return false;
    };

    // ── Connexion initiale (plusieurs essais) ─────────────────────────────────
    uint8_t attempts = 0;
    while (!connect()) {
        attempts++;
        Serial.printf("[WIFI] Tentative %u/3 échouée — attente 5s\n", attempts);
        if (attempts >= 3) {
            Serial.println("[WIFI] WiFi indisponible — serveur web désactivé");
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    Serial.printf("[WIFI] IP : http://%s\n", WiFi.localIP().toString().c_str());

    // ── mDNS ─────────────────────────────────────────────────────────────────
    if (MDNS.begin("robo-omni")) {
        Serial.println("[WIFI] mDNS : http://robo-omni.local");
    }

    // ── Démarrer le serveur HTTP ──────────────────────────────────────────────
    self->server.begin();
    self->_ready = true;
    Serial.printf("[WEB] Serveur démarré sur le port %u\n", WEB_PORT);

    // ── Broadcast UDP : robot annonce son IP sur le réseau ───────────────────
    // Permet à un téléphone Android (mDNS non supporté) de trouver le robot
    // Port 54321 — message "ROBO-OMNI:192.168.x.x:80"
    WiFiUDP udp;
    String discovery_msg = "ROBO-OMNI:" + WiFi.localIP().toString() + ":80";
    Serial.printf("[WIFI] Broadcast découverte : %s\n", discovery_msg.c_str());
    Serial.println("[WIFI] ─────────────────────────────────────────");
    Serial.printf("[WIFI]  Accès direct : http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("[WIFI]  Android     : utiliser l'IP ci-dessus");
    Serial.println("[WIFI]  iOS / Mac   : http://robo-omni.local");
    Serial.println("[WIFI] ─────────────────────────────────────────");

    uint32_t last_broadcast = 0;

    // ── Boucle de surveillance reconnexion + broadcast ────────────────────────
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Broadcast UDP toutes les 10s
        if (millis() - last_broadcast > 10000) {
            udp.beginPacket(IPAddress(255,255,255,255), 54321);
            udp.print(discovery_msg);
            udp.endPacket();
            last_broadcast = millis();
        }

        if (WiFi.status() != WL_CONNECTED) {
            self->_ready = false;
            Serial.println("[WIFI] Connexion perdue — reconnexion...");

            WiFi.disconnect();
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (connect()) {
                MDNS.begin("robo-omni");
                discovery_msg = "ROBO-OMNI:" + WiFi.localIP().toString() + ":80";
                self->_ready  = true;
                Serial.printf("[WIFI] Reconnecté : http://%s\n",
                              WiFi.localIP().toString().c_str());
            }
        }
    }
}

// ─── Configuration des routes ──────────────────────────────────────────────────

void WebServer::setupRoutes() {

    // GET /
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "text/html", buildMainPage());
    });

    // GET /sensors
    server.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "application/json", toSensorJson());
    });

    // GET /stream placeholder
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(501, "text/plain", "Stream non implémenté");
    });

    // POST /control — body JSON {vx, vy, wz}
    server.on("/control", HTTP_POST,
        [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "OK"); },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data,
               size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, (char*)data, len) != DeserializationError::Ok) return;
            float vx = doc["vx"] | 0.0f;
            float vy = doc["vy"] | 0.0f;
            float wz = doc["wz"] | 0.0f;
            if (command_cb) command_cb(vx, vy, wz);
        }
    );

    // GET /update — page OTA
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", R"=====(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ROBO-OMNI OTA</title>
<style>body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;display:flex;flex-direction:column;align-items:center;padding:24px}
h1{color:#0af}.box{background:#16213e;border-radius:12px;padding:24px;width:100%;max-width:400px}
input[type=file]{color:#eee;margin:12px 0;width:100%}
button{background:#0af;color:#000;border:none;border-radius:8px;padding:10px 24px;font-size:16px;cursor:pointer;width:100%}
button:disabled{opacity:.4}#bar-wrap{background:#0a0a1a;border-radius:8px;height:20px;width:100%;margin:12px 0;display:none}
#bar{height:100%;width:0;background:#0af;border-radius:8px;transition:width .3s;text-align:center;font-size:12px;line-height:20px}
#msg{text-align:center;margin-top:8px}a{color:#0af;font-size:13px}</style></head>
<body><h1>ROBO-OMNI OTA</h1>
<div class="box">
<p>Fichier <b>.bin</b> PlatformIO :<br><small style="color:#888">.pio/build/xiao_esp32s3_sense/firmware.bin</small></p>
<input type="file" id="fw" accept=".bin">
<div id="bar-wrap"><div id="bar">0%</div></div>
<button id="btn" onclick="flash()">⚡ Flasher</button>
<div id="msg"></div><br><a href="/">← Retour</a></div>
<script>
function flash(){
  const file=document.getElementById('fw').files[0];
  if(!file){alert('Sélectionne un .bin');return;}
  const btn=document.getElementById('btn'),bar=document.getElementById('bar'),
        wrap=document.getElementById('bar-wrap'),msg=document.getElementById('msg');
  btn.disabled=true;wrap.style.display='block';msg.textContent='Envoi…';
  const fd=new FormData();fd.append('firmware',file,file.name);
  const xhr=new XMLHttpRequest();
  xhr.upload.onprogress=e=>{const p=Math.round(e.loaded/e.total*100);bar.style.width=p+'%';bar.textContent=p+'%';};
  xhr.onload=()=>{if(xhr.status===200){msg.textContent='✅ OK — redémarrage…';bar.style.background='#0f0';}
    else{msg.textContent='❌ '+xhr.responseText;bar.style.background='#f00';btn.disabled=false;}};
  xhr.onerror=()=>{msg.textContent='❌ Connexion perdue';btn.disabled=false;};
  xhr.open('POST','/update');xhr.send(fd);}
</script></body></html>)=====");
    });

    // POST /update — flash OTA
    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            bool ok = !Update.hasError();
            auto* r = req->beginResponse(ok ? 200 : 500, "text/plain",
                                         ok ? "OK" : Update.errorString());
            r->addHeader("Connection", "close");
            req->send(r);
            if (ok) { vTaskDelay(pdMS_TO_TICKS(500)); ESP.restart(); }
        },
        [](AsyncWebServerRequest* req, String filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                Serial.printf("[OTA] Début : %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
                    Update.printError(Serial);
            }
            if (Update.write(data, len) != len) Update.printError(Serial);
            if (final) {
                if (Update.end(true))
                    Serial.printf("[OTA] OK — %u octets\n", index + len);
                else
                    Update.printError(Serial);
            }
        }
    );

    // 404
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });
}

// ─── Helpers JSON / HTML ───────────────────────────────────────────────────────

String WebServer::toSensorJson() {
    JsonDocument doc;
    doc["tof_front"] = (uint16_t)cache_tof;
    doc["tof_left"]  = 0;
    doc["tof_right"] = 0;
    doc["roll"]      = (float)cache_roll;
    doc["pitch"]     = (float)cache_pitch;
    String out;
    serializeJson(doc, out);
    return out;
}

String WebServer::buildMainPage() {
    return R"=====(<!DOCTYPE html>
<html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ROBO-OMNI</title>
<style>
body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:0}
h1{text-align:center;color:#0af;margin:16px 0 4px}
.sub{text-align:center;color:#888;font-size:13px;margin-bottom:16px}
.panel{background:#16213e;border-radius:12px;padding:16px;margin:12px auto;max-width:320px}
.joy-wrap{display:flex;justify-content:center}
svg{touch-action:none;cursor:grab}
.badge{display:inline-block;background:#0af2;color:#0af;border:1px solid #0af4;border-radius:4px;padding:2px 8px;margin:2px;font-family:monospace;font-size:13px}
</style></head>
<body>
<h1>ROBO-OMNI</h1>
<p class="sub">Joystick &nbsp;|&nbsp; <a href="/update" style="color:#0af">⚡ OTA</a></p>
<div class="panel"><div class="joy-wrap">
  <svg id="svg" width="220" height="220" viewBox="0 0 220 220">
    <circle cx="110" cy="110" r="100" fill="#0a0a1a" stroke="#0af" stroke-width="2"/>
    <line x1="110" y1="10" x2="110" y2="210" stroke="#0af3" stroke-width="1"/>
    <line x1="10" y1="110" x2="210" y2="110" stroke="#0af3" stroke-width="1"/>
    <circle id="thumb" cx="110" cy="110" r="28" fill="#0af" opacity="0.9"/>
  </svg>
</div></div>
<div class="panel"><b>Capteurs</b><div id="sens" style="margin-top:8px">—</div></div>
<script>
const svg=document.getElementById('svg'),thumb=document.getElementById('thumb');
const CX=110,CY=110,MAX_R=72;let drag=false;
function pt(e){const r=svg.getBoundingClientRect(),s=e.touches?e.touches[0]:e;return{x:s.clientX-r.left,y:s.clientY-r.top};}
function start(e){e.preventDefault();drag=true;}
function end(e){drag=false;thumb.setAttribute('cx',CX);thumb.setAttribute('cy',CY);send(0,0,0);}
function move(e){
  if(!drag)return;e.preventDefault();
  const p=pt(e),dx=p.x-CX,dy=p.y-CY,len=Math.sqrt(dx*dx+dy*dy),c=Math.min(len,MAX_R);
  const nx=len>0?dx/len:0,ny=len>0?dy/len:0;
  thumb.setAttribute('cx',CX+nx*c);thumb.setAttribute('cy',CY+ny*c);
  send(-ny*(c/MAX_R),nx*(c/MAX_R),0);}
svg.addEventListener('mousedown',start);svg.addEventListener('touchstart',start,{passive:false});
document.addEventListener('mouseup',end);document.addEventListener('touchend',end);
document.addEventListener('mousemove',move);document.addEventListener('touchmove',move,{passive:false});
function send(vx,vy,wz){fetch('/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({vx,vy,wz})}).catch(()=>{});}
setInterval(()=>{fetch('/sensors').then(r=>r.json()).then(d=>{
  document.getElementById('sens').innerHTML=
    '<span class="badge">TOF '+d.tof_front+' mm</span> '+
    '<span class="badge">Roll '+d.roll.toFixed(1)+'°</span> '+
    '<span class="badge">Pitch '+d.pitch.toFixed(1)+'°</span>';
}).catch(()=>{});},300);
</script></body></html>)=====";
}
