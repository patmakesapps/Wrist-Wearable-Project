#include "link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <string.h>

namespace {
constexpr size_t MAX_PKT = 64;
const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t rxBuf[MAX_PKT];
size_t  rxLen = 0;
uint8_t rxMac[6];
int     rxRssi = 0;
bool    rxFresh = false;

// Runs on the Wi-Fi task. Copy out quickly, let loop() do the work.
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len <= 0 || (size_t)len > MAX_PKT) return;
    portENTER_CRITICAL(&rxMux);
    memcpy(rxBuf, data, (size_t)len);
    rxLen = (size_t)len;
    memcpy(rxMac, info->src_addr, 6);
    rxRssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    rxFresh = true;
    portEXIT_CRITICAL(&rxMux);
}
}

bool Link::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);              // modem sleep adds tens of ms of receive latency
    // Lower TX power: the default 19.5 dBm pulls a ~350 mA burst that browns out a weak
    // battery feed. 11 dBm is plenty for two boards in the same room.
    WiFi.setTxPower(WIFI_POWER_11dBm);
    if (esp_now_init() != ESP_OK) return false;
    if (esp_now_register_recv_cb(onReceive) != ESP_OK) return false;
    return ensurePeer(BROADCAST);
}

bool Link::ensurePeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;                  // current channel
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool Link::send(const uint8_t mac[6], const void *data, size_t len) {
    if (!ensurePeer(mac)) return false;
    return esp_now_send(mac, (const uint8_t *)data, len) == ESP_OK;
}

bool Link::broadcast(const void *data, size_t len) {
    return send(BROADCAST, data, len);
}

bool Link::receive(uint8_t *buf, size_t bufLen, size_t &outLen, uint8_t srcMac[6], int *rssi) {
    portENTER_CRITICAL(&rxMux);
    if (!rxFresh) { portEXIT_CRITICAL(&rxMux); return false; }
    size_t n = rxLen < bufLen ? rxLen : bufLen;
    memcpy(buf, rxBuf, n);
    memcpy(srcMac, rxMac, 6);
    if (rssi) *rssi = rxRssi;
    rxFresh = false;
    portEXIT_CRITICAL(&rxMux);
    outLen = n;
    return true;
}

// WiFi.macAddress() reads the netif, which is empty until an AP connection; use the efuse MAC.
String Link::localMac() const {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char s[18];
    macToString(mac, s);
    return String(s);
}

void Link::macToString(const uint8_t mac[6], char out[18]) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
