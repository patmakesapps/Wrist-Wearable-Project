#pragma once
#include <Arduino.h>

// Thin ESP-NOW wrapper. Keeps only the most recent received packet; the main loop
// polls it. Good enough for a control stream where stale commands should be dropped.
class Link {
public:
    bool begin();                                                    // Wi-Fi STA + ESP-NOW, no AP needed
    bool send(const uint8_t mac[6], const void *data, size_t len);  // unicast (peer auto-added)
    bool broadcast(const void *data, size_t len);

    // True if a packet arrived since the last call; copies it out.
    bool receive(uint8_t *buf, size_t bufLen, size_t &outLen, uint8_t srcMac[6], int *rssi = nullptr);

    String localMac() const;
    static void macToString(const uint8_t mac[6], char out[18]);

private:
    bool ensurePeer(const uint8_t mac[6]);
};
