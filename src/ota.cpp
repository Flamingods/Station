#include "ota.h"
#include <ArduinoOTA.h>
#include "config.h"
#include "leds.h"
#include "network.h"

// handle() runs the whole upload, so the loop (and its streaming) stops until it ends.
// Release first so the Flamingo is not left holding a pressed mask.
static void sendRelease() {
    uint8_t packet[PACKET_SIZE];
    encodePacket(STATION_ID, 0, packet);
    for (uint8_t i = 0; i < RELEASE_REPEATS; i++) network_send(packet, PACKET_SIZE);
}

void ota_begin() {
    ArduinoOTA.setHostname(STATION_NAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([] {
        Serial.println("OTA start");
        sendRelease();
    });
    ArduinoOTA.onEnd([] { Serial.println("\nOTA done, rebooting"); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        Serial.printf("OTA %u%%\r", done * 100 / total);
        leds_progress(done, total);
    });
    ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA error %u\n", error); });
    ArduinoOTA.begin();
    Serial.printf("OTA ready: %s.local\n", STATION_NAME);
}

void ota_handle() { ArduinoOTA.handle(); }
