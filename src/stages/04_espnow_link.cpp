// Stage 4: prove the radio link. Flash this SAME firmware to both ESP32s.
// Each board prints its MAC, broadcasts a heartbeat every 500 ms, and prints every
// heartbeat it hears with the sender's MAC and signal strength.
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "link.h"
#include "packets.h"

static Link        radio;
static uint16_t seq = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Stage 4: ESP-NOW link test (flash to BOTH boards) ===");
    if (!radio.begin()) {
        Serial.println("ESP-NOW init failed");
        while (true) delay(1000);
    }
    Serial.printf("This board's MAC: %s\n", radio.localMac().c_str());
    Serial.println("Broadcasting a heartbeat every 500 ms; printing any heartbeat received.");
}

void loop() {
    static uint32_t lastTx = 0;
    const uint32_t now = millis();

    if (now - lastTx >= 500) {
        lastTx = now;
        HeartbeatPacket hb = {HEARTBEAT_MAGIC, seq++, now};
        bool ok = radio.broadcast(&hb, sizeof hb);
        Serial.printf("[%6u ms] tx heartbeat #%u %s\n", (unsigned)now, hb.seq, ok ? "" : "FAILED");
    }

    uint8_t buf[64];
    size_t len = 0;
    uint8_t mac[6];
    int rssi = 0;
    if (radio.receive(buf, sizeof buf, len, mac, &rssi)) {
        char ms[18];
        Link::macToString(mac, ms);
        if (len == sizeof(HeartbeatPacket)) {
            HeartbeatPacket hb;
            memcpy(&hb, buf, sizeof hb);
            if (hb.magic == HEARTBEAT_MAGIC) {
                Serial.printf("[%6u ms] rx heartbeat #%u from %s  uptime %u ms  rssi %d dBm\n",
                              (unsigned)now, hb.seq, ms, (unsigned)hb.uptime_ms, rssi);
                return;
            }
        }
        Serial.printf("[%6u ms] rx %u bytes from %s (not a heartbeat)\n", (unsigned)now, (unsigned)len, ms);
    }
}
