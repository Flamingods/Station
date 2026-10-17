#include "web.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <esp_timer.h>
#include "Parse.h"
#include "SimHold.h"
#include "config.h"
#include "network.h"
#include "stats.h"

static WebServer server(80);
static SimHold simHold;

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Station</title>
<style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:480px;margin:0 auto;padding:16px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
.b{flex:1;min-width:64px;padding:18px 0;border:3px solid #333;border-radius:10px;font-weight:600;color:#000}
.b.on{border-color:#fff;box-shadow:0 0 12px #fff8}
button{font-size:16px;padding:8px 12px}
#msg{min-height:1.2em;color:#f88}
#st{font-family:monospace;white-space:pre;background:#222;padding:12px;border-radius:8px;overflow-x:auto}
</style></head><body>
<h2 id="t">Station</h2>
<div class="row" id="btns"></div>
<div class="row"><label>Hold ms <input id="ms" type="number" value="1000" min="1" max="10000"></label></div>
<div class="row">
<button onclick="send(sel)">Hold selected</button>
<button onclick="send(31)">All 5 (special)</button>
<button onclick="send(0)">Release</button>
</div>
<div id="msg"></div>
<div id="st">loading...</div>
<script>
const C=[['red','#e33'],['green','#3c3'],['blue','#36f'],['yellow','#ec3'],['white','#ddd']];
let sel=0;
const box=document.getElementById('btns');
C.forEach(([n,c],i)=>{const b=document.createElement('button');b.className='b';b.textContent=n;
b.style.background=c;b.onclick=()=>{sel^=1<<i;b.classList.toggle('on',!!(sel&1<<i));};box.appendChild(b);});
async function send(m){
  const ms=document.getElementById('ms').value;
  const r=await fetch('/press?mask='+m+'&ms='+ms,{method:'POST'});
  document.getElementById('msg').textContent=r.ok?'':await r.text();
}
async function poll(){
  try{const s=await (await fetch('/status')).json();
  document.getElementById('t').textContent=s.station_name;
  document.getElementById('st').textContent=JSON.stringify(s,null,1);}catch(e){}
}
setInterval(poll,1000);poll();
</script></body></html>)HTML";

static void handleStatus() {
    StaticJsonDocument<768> doc;
    doc["station_id"] = STATION_ID;
    doc["station_name"] = STATION_NAME;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["network_type"] = network_type();
    doc["ip"] = network_ip();
    doc["mac"] = network_mac();
    if (strcmp(network_type(), "wifi") == 0) doc["rssi"] = network_rssi();
    doc["uptime_s"] = uint32_t(esp_timer_get_time() / 1000000);  // 64-bit: millis() wraps at 49.7 days
    doc["current_mask"] = stats.currentMask;
    doc["press_count"] = stats.pressCount;
    doc["packets_sent"] = stats.packetsSent;
    doc["flamingo_host"] = FLAMINGO_HOST;
    doc["flamingo_ip"] = network_flamingo_ip().toString();
    doc["flamingo_port"] = FLAMINGO_PORT;

    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
}

static void handlePress() {
    const String maskArg = server.arg("mask");
    const String msArg = server.arg("ms");
    uint8_t mask;
    uint32_t ms;
    if (!parseMask(server.hasArg("mask") ? maskArg.c_str() : nullptr, &mask)) {
        server.send(400, "application/json", "{\"error\":\"mask must be 0..0x1F\"}");
        return;
    }
    if (!parseDurationMs(server.hasArg("ms") ? msArg.c_str() : nullptr, SIM_DEFAULT_MS, SIM_MAX_MS, &ms)) {
        server.send(400, "application/json", "{\"error\":\"ms must be a positive integer\"}");
        return;
    }
    simHold.start(mask, ms, millis());
    Serial.printf("Web: hold 0x%02X for %lu ms\n", mask, (unsigned long)ms);

    char body[48];
    snprintf(body, sizeof(body), "{\"mask\":%u,\"ms\":%lu}", mask, (unsigned long)ms);
    server.send(200, "application/json", body);
}

void web_begin() {
    server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/press", HTTP_POST, handlePress);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Web server on port 80");
}

void web_handle() { server.handleClient(); }

uint8_t web_simulated_mask(uint32_t nowMs) { return simHold.mask(nowMs); }
