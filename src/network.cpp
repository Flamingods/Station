#include "network.h"
#include <ESPmDNS.h>
#include <ETH.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_event.h>
#include "config.h"

static volatile bool ethConnected = false;  // written from the WiFi event task
static bool mdnsStarted = false;
// The Flamingo's address is resolved by resolverTask so a slow or failing
// lookup (mDNS can block ~7 s per try) never stalls buttons, LEDs, web or OTA.
// Hostnames are re-resolved every FLAMINGO_REFRESH_MS in case the Flamingo's DHCP
// address changes; a failed refresh keeps the last known address.
static volatile uint32_t flamingoAddr = 0;      // 0 = unresolved; written by resolverTask
static volatile uint32_t resolveGeneration = 0;  // bumped by the loop on network change
static uint32_t lastRetryMs = 0;
static WiFiUDP udp;

static bool ethUp() { return ethConnected; }
static bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

static void onEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname(STATION_NAME);
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            ethConnected = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
        case ARDUINO_EVENT_ETH_STOP:
            ethConnected = false;
            break;
        default:
            break;
    }
}

// The 2.x core does not forward this one: the DHCP lease ran out with the cable still in.
static void onEthLostIp(void*, esp_event_base_t, int32_t, void*) { ethConnected = false; }

static bool waitFor(bool (*up)(), uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (!up() && millis() - start < timeoutMs) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    return up();
}

static void startWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(STATION_NAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void resolverTask(void*) {
    IPAddress literal;
    const bool isLiteral = literal.fromString(FLAMINGO_HOST);
    uint32_t resolvedAtMs = 0;
    for (;;) {
        const bool known = flamingoAddr != 0;
        const bool due = !known || (!isLiteral && millis() - resolvedAtMs >= FLAMINGO_REFRESH_MS);
        if (network_connected() && due) {
            const uint32_t generation = resolveGeneration;
            IPAddress ip = literal;
            const bool ok = isLiteral || WiFi.hostByName(FLAMINGO_HOST, ip);
            if (ok && uint32_t(ip) != 0) {
                // Drop the result if the network changed while we were resolving.
                if (generation == resolveGeneration) {
                    if (uint32_t(ip) != flamingoAddr) {
                        Serial.printf("Flamingo %s -> %s:%u\n", FLAMINGO_HOST, ip.toString().c_str(),
                                      FLAMINGO_PORT);
                    }
                    flamingoAddr = uint32_t(ip);
                    resolvedAtMs = millis();
                }
            } else {
                Serial.printf("Could not resolve %s%s, retrying in %u s\n", FLAMINGO_HOST,
                              known ? " (keeping last address)" : "",
                              (unsigned)(FLAMINGO_RESOLVE_RETRY_MS / 1000));
                vTaskDelay(pdMS_TO_TICKS(FLAMINGO_RESOLVE_RETRY_MS));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void network_begin() {
    WiFi.onEvent(onEvent);
    xTaskCreate(resolverTask, "flamingo-dns", 4096, nullptr, 1, nullptr);

    pinMode(ETH01_POWER_ENABLE_PIN, OUTPUT);
    digitalWrite(ETH01_POWER_ENABLE_PIN, HIGH);
    delay(500);

    Serial.print("Ethernet");
    if (!ETH.begin(ETH01_PHY_ADDR, ETH01_PHY_POWER_PIN, ETH01_MDC_PIN, ETH01_MDIO_PIN,
                   ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN)) {
        Serial.print(" (ETH.begin failed)");
    }
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, onEthLostIp, nullptr);
    if (waitFor(ethUp, CONNECT_TIMEOUT_MS)) return;

    Serial.printf("WiFi '%s'", WIFI_SSID);
    startWifi();
    if (!waitFor(wifiUp, CONNECT_TIMEOUT_MS)) {
        Serial.println("No network yet - will keep retrying");
    }
}

void network_loop(uint32_t nowMs) {
    static bool wasConnected = false;
    const bool connected = network_connected();
    if (connected != wasConnected) {
        wasConnected = connected;
        Serial.printf("Network %s: %s %s\n", connected ? "up" : "down", network_type(),
                      network_ip().c_str());
        resolveGeneration++;
        flamingoAddr = 0;  // re-resolve on the new link
    }

    // Prefer Ethernet: drop WiFi once the cable is up so routing is unambiguous.
    if (ethConnected && WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    if (!connected) {
        if (nowMs - lastRetryMs >= NETWORK_RETRY_MS) {
            lastRetryMs = nowMs;
            Serial.println("Offline - retrying WiFi");
            if (WiFi.getMode() == WIFI_OFF) startWifi();
            else WiFi.reconnect();
        }
        return;
    }

    if (!mdnsStarted) {
        mdnsStarted = MDNS.begin(STATION_NAME);
        if (mdnsStarted) Serial.printf("mDNS: %s.local\n", STATION_NAME);
    }
}

bool network_connected() { return ethConnected || wifiUp(); }

const char* network_type() {
    if (ethConnected) return "ethernet";
    if (wifiUp()) return "wifi";
    return "disconnected";
}

String network_ip() {
    if (ethConnected) return ETH.localIP().toString();
    if (wifiUp()) return WiFi.localIP().toString();
    return "0.0.0.0";
}

String network_mac() { return ethConnected ? ETH.macAddress() : WiFi.macAddress(); }

int32_t network_rssi() { return !ethConnected && wifiUp() ? WiFi.RSSI() : 0; }

IPAddress network_flamingo_ip() { return IPAddress(flamingoAddr); }

bool network_send(const uint8_t* data, size_t len) {
    const uint32_t addr = flamingoAddr;
    if (!network_connected() || addr == 0) return false;
    if (!udp.beginPacket(IPAddress(addr), FLAMINGO_PORT)) return false;
    udp.write(data, len);
    return udp.endPacket() == 1;
}
