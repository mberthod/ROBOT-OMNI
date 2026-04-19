/**
 * @file web_server.cpp
 * @brief Implémentation du serveur web ROBO-OMNI
 * @author Mathieu (Robot Omni)
 * @date 17/04/2026
 * @version 1.0
 */

#include "web_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>   // OTA firmware update natif ESP32

/**
 * @brief Connecte au WiFi et démarre HTTP + mDNS
 */
bool WebServer::begin(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.printf("[WIFI] Connexion à '%s'", ssid);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println(" TIMEOUT");
            Serial.printf("[ERREUR] SSID '%s' introuvable\n", ssid);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println(" OK");
    Serial.printf("[WIFI] IP : http://%s\n", WiFi.localIP().toString().c_str());

    // mDNS → accessible via http://robo-omni.local
    if (MDNS.begin("robo-omni")) {
        Serial.println("[WIFI] mDNS : http://robo-omni.local");
    }

    // ── GET / ────────────────────────────────────────────────────────────────
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "text/html", buildMainPage());
    });

    // ── GET /sensors ─────────────────────────────────────────────────────────
    server.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "application/json", toSensorJson());
    });

    // ── GET /stream placeholder ───────────────────────────────────────────────
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(501, "text/plain", "Stream non implementé");
    });

    // ── POST /control : JSON body {vx, vy, wz} ───────────────────────────────
    // ESPAsyncWebServer : le body JSON arrive dans le 3ème lambda (onBody)
    server.on(
        "/control",
        HTTP_POST,
        // handler exécuté après réception complète
        [](AsyncWebServerRequest* req) {
            req->send(200, "text/plain", "OK");
        },
        // upload handler (non utilisé)
        nullptr,
        // body handler : reçoit les octets du corps de la requête
        [this](AsyncWebServerRequest* req, uint8_t* data,
               size_t len, size_t index, size_t total) {
            String body = String(reinterpret_cast<char*>(data), len);
            JsonDocument doc;
            if (deserializeJson(doc, body) != DeserializationError::Ok) return;
            float vx = doc["vx"] | 0.0f;
            float vy = doc["vy"] | 0.0f;
            float wz = doc["wz"] | 0.0f;
            if (command_cb) command_cb(vx, vy, wz);
        }
    );

    // ── GET /update : page HTML upload firmware ───────────────────────────────
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", R"=====(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ROBO-OMNI — OTA</title>
<style>
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;
       display:flex;flex-direction:column;align-items:center;padding:24px}
  h1{color:#0af}
  .box{background:#16213e;border-radius:12px;padding:24px;width:100%;max-width:400px}
  input[type=file]{color:#eee;margin:12px 0;width:100%}
  button{background:#0af;color:#000;border:none;border-radius:8px;
         padding:10px 24px;font-size:16px;cursor:pointer;width:100%}
  button:disabled{opacity:.4;cursor:not-allowed}
  #bar-wrap{background:#0a0a1a;border-radius:8px;height:20px;
            width:100%;margin:12px 0;display:none}
  #bar{height:100%;width:0;background:#0af;border-radius:8px;
       transition:width .3s;text-align:center;font-size:12px;line-height:20px}
  #msg{text-align:center;margin-top:8px;min-height:20px}
  a{color:#0af;font-size:13px}
</style>
</head>
<body>
<h1>ROBO-OMNI OTA</h1>
<div class="box">
  <p>Sélectionne le fichier <b>.bin</b> PlatformIO :<br>
     <small style="color:#888">.pio/build/xiao_esp32s3_sense/firmware.bin</small></p>
  <input type="file" id="fw" accept=".bin">
  <div id="bar-wrap"><div id="bar">0%</div></div>
  <button id="btn" onclick="flash()">⚡ Flasher</button>
  <div id="msg"></div>
  <br><a href="/">← Retour au contrôle</a>
</div>
<script>
function flash() {
  const file = document.getElementById('fw').files[0];
  if (!file) { alert('Sélectionne un fichier .bin'); return; }
  const btn  = document.getElementById('btn');
  const bar  = document.getElementById('bar');
  const wrap = document.getElementById('bar-wrap');
  const msg  = document.getElementById('msg');
  btn.disabled = true;
  wrap.style.display = 'block';
  msg.textContent = 'Envoi en cours…';
  const fd = new FormData();
  fd.append('firmware', file, file.name);
  const xhr = new XMLHttpRequest();
  xhr.upload.onprogress = e => {
    const pct = Math.round(e.loaded / e.total * 100);
    bar.style.width = pct + '%';
    bar.textContent = pct + '%';
  };
  xhr.onload = () => {
    if (xhr.status === 200) {
      msg.textContent = '✅ Flash OK — redémarrage dans 3s…';
      bar.style.background = '#0f0';
    } else {
      msg.textContent = '❌ Erreur : ' + xhr.responseText;
      bar.style.background = '#f00';
      btn.disabled = false;
    }
  };
  xhr.onerror = () => {
    msg.textContent = '❌ Connexion perdue (redémarrage ?)';
    btn.disabled = false;
  };
  xhr.open('POST', '/update');
  xhr.send(fd);
}
</script>
</body>
</html>)=====");
    });

    // ── POST /update : réception + flash firmware ─────────────────────────────
    server.on(
        "/update",
        HTTP_POST,
        // Appelé après upload complet
        [](AsyncWebServerRequest* req) {
            bool ok = !Update.hasError();
            AsyncWebServerResponse* resp = req->beginResponse(
                ok ? 200 : 500,
                "text/plain",
                ok ? "OK" : Update.errorString()
            );
            resp->addHeader("Connection", "close");
            req->send(resp);
            if (ok) {
                delay(500);
                ESP.restart();
            }
        },
        // Upload handler : reçoit les chunks du .bin
        [](AsyncWebServerRequest* req, String filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                Serial.printf("[OTA] Début flash : %s (%u octets)\n",
                              filename.c_str(), req->contentLength());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] ✅ Flash OK — %u octets\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // ── 404 par défaut ────────────────────────────────────────────────────────
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.printf("[WEB] Serveur démarré sur le port %d\n", WEB_PORT);
    return true;
}

/**
 * @brief Définit la callback commande de mouvement
 */
void WebServer::setCommandCallback(void (*cb)(float vx, float vy, float wz)) {
    command_cb = cb;
}

/**
 * @brief Met à jour le cache capteurs (appelé depuis loop())
 */
void WebServer::sendSensorData(uint16_t tof_f, uint16_t tof_l, uint16_t tof_r,
                                float roll, float pitch) {
    cache_tof_f  = tof_f;
    cache_tof_l  = tof_l;
    cache_tof_r  = tof_r;
    cache_roll   = roll;
    cache_pitch  = pitch;
}

/**
 * @brief Retourne le JSON capteurs depuis le cache
 */
String WebServer::toSensorJson() {
    JsonDocument doc;
    doc["tof_front"] = cache_tof_f;
    doc["tof_left"]  = cache_tof_l;
    doc["tof_right"] = cache_tof_r;
    doc["roll"]      = cache_roll;
    doc["pitch"]     = cache_pitch;
    String out;
    serializeJson(doc, out);
    return out;
}

/**
 * @brief Page HTML principale avec joystick SVG et télémétrie
 */
String WebServer::buildMainPage() {
    // Construit en RAM (pas en PROGMEM pour rester simple)
    String html = R"=====(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ROBO-OMNI</title>
<style>
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:0}
  h1{text-align:center;color:#0af;margin:16px 0 4px}
  .sub{text-align:center;color:#888;font-size:13px;margin-bottom:16px}
  .panel{background:#16213e;border-radius:12px;padding:16px;margin:12px auto;max-width:320px}
  .joy-wrap{display:flex;justify-content:center}
  svg{touch-action:none;cursor:grab}
  .sensor{font-family:monospace;font-size:13px;line-height:1.8}
  .badge{display:inline-block;background:#0af2;color:#0af;
         border:1px solid #0af4;border-radius:4px;padding:2px 8px;margin:2px}
</style>
</head>
<body>
<h1>ROBO-OMNI</h1>
<p class="sub">Joystick de contrôle &nbsp;|&nbsp; <a href="/update" style="color:#0af;font-size:13px">⚡ OTA Update</a></p>

<div class="panel">
  <div class="joy-wrap">
    <svg id="svg" width="220" height="220" viewBox="0 0 220 220">
      <circle cx="110" cy="110" r="100" fill="#0a0a1a" stroke="#0af" stroke-width="2"/>
      <line x1="110" y1="10"  x2="110" y2="210" stroke="#0af3" stroke-width="1"/>
      <line x1="10"  y1="110" x2="210" y2="110" stroke="#0af3" stroke-width="1"/>
      <circle id="thumb" cx="110" cy="110" r="28" fill="#0af" opacity="0.9"/>
    </svg>
  </div>
</div>

<div class="panel">
  <b>Capteurs</b>
  <div class="sensor" id="sens">—</div>
</div>

<script>
const svg   = document.getElementById('svg');
const thumb = document.getElementById('thumb');
const CX = 110, CY = 110, MAX_R = 72;
let dragging = false, lastVx = 0, lastVy = 0;

function ptFromEvent(e) {
  const r = svg.getBoundingClientRect();
  const src = e.touches ? e.touches[0] : e;
  return { x: src.clientX - r.left, y: src.clientY - r.top };
}

function startDrag(e) { e.preventDefault(); dragging = true; }
function endDrag(e)   {
  dragging = false;
  thumb.setAttribute('cx', CX);
  thumb.setAttribute('cy', CY);
  sendCmd(0, 0, 0);
}
function moveDrag(e) {
  if (!dragging) return;
  e.preventDefault();
  const p   = ptFromEvent(e);
  const dx  = p.x - CX;
  const dy  = p.y - CY;
  const len = Math.sqrt(dx*dx + dy*dy);
  const clamped = Math.min(len, MAX_R);
  const nx  = len > 0 ? dx / len : 0;
  const ny  = len > 0 ? dy / len : 0;
  thumb.setAttribute('cx', CX + nx * clamped);
  thumb.setAttribute('cy', CY + ny * clamped);
  const ratio = clamped / MAX_R;
  lastVx = -ny * ratio;   // haut = avant
  lastVy =  nx * ratio;   // droite = vy positif
  sendCmd(lastVx, lastVy, 0);
}

svg.addEventListener('mousedown',  startDrag);
svg.addEventListener('touchstart', startDrag, {passive:false});
document.addEventListener('mouseup',   endDrag);
document.addEventListener('touchend',  endDrag);
document.addEventListener('mousemove', moveDrag);
document.addEventListener('touchmove', moveDrag, {passive:false});

function sendCmd(vx, vy, wz) {
  fetch('/control', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify({vx, vy, wz})
  }).catch(() => {});
}

setInterval(() => {
  fetch('/sensors').then(r => r.json()).then(d => {
    document.getElementById('sens').innerHTML =
      '<span class="badge">Front ' + d.tof_front + ' mm</span>' +
      '<span class="badge">Left '  + d.tof_left  + ' mm</span>' +
      '<span class="badge">Right ' + d.tof_right + ' mm</span><br>' +
      '<span class="badge">Roll '  + d.roll.toFixed(1)  + '°</span>' +
      '<span class="badge">Pitch ' + d.pitch.toFixed(1) + '°</span>';
  }).catch(() => {});
}, 300);
</script>
</body>
</html>)=====";
    return html;
}
