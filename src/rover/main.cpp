// Rover: receives ControlPackets over ESP-NOW, mixes throttle/steer into two motors on
// the Adafruit Motor Bonnet, and stops by itself if commands stop arriving. Sends
// telemetry back to whoever is commanding it. Works without the bonnet for link testing.
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include "config.h"
#include "link.h"
#include "bonnet.h"
#include "packets.h"

static Link        radio;
static MotorBonnet bonnet(BONNET_ADDR);
static bool        bonnetOk = false;

static uint8_t  ctrlMac[6] = {0};
static bool     haveCtrl = false;
static uint32_t lastCmdAt = 0;
static bool     cmdTimeout = true;
static ControlPacket lastCmd = {};

static int targetL = 0, targetR = 0;   // after mixing, -100..100
static int curL = 0, curR = 0;         // after slew limiting
static int appliedL = 999, appliedR = 999;

// N20s do nothing below ~25 %: map 1..100 onto MOTOR_MIN_PERCENT..100.
static int applyMinDuty(int pct) {
    if (pct == 0) return 0;
    const int mag = pct < 0 ? -pct : pct;
    const int out = MOTOR_MIN_PERCENT + (100 - MOTOR_MIN_PERCENT) * mag / 100;
    return pct < 0 ? -out : out;
}

// Arcade mix: + steer turns right (left wheel faster).
static void mix(int thr, int str, int &l, int &r) {
    l = thr + str;
    r = thr - str;
    const int m = max(abs(l), abs(r));
    if (m > 100) { l = l * 100 / m; r = r * 100 / m; }
}

static int slew(int cur, int target) {
    const int step = MOTOR_SLEW_PER_TICK;
    if (target > cur) return min(target, cur + step);
    if (target < cur) return max(target, cur - step);
    return cur;
}

static void applyMotors() {
    if (!bonnetOk) return;
    const int l = applyMinDuty(curL) * MOTOR_LEFT_SIGN;
    const int r = applyMinDuty(curR) * MOTOR_RIGHT_SIGN;
    if (l != appliedL) { bonnet.setMotor(MOTOR_LEFT, l);  appliedL = l; }
    if (r != appliedR) { bonnet.setMotor(MOTOR_RIGHT, r); appliedR = r; }
}

static void handleCommands(uint32_t now) {
    uint8_t buf[64];
    size_t len = 0;
    uint8_t mac[6];
    if (!radio.receive(buf, sizeof buf, len, mac)) return;
    if (len != sizeof(ControlPacket)) return;
    ControlPacket p;
    memcpy(&p, buf, sizeof p);
    if (p.magic != CONTROL_MAGIC) return;

    if (!haveCtrl || memcmp(mac, ctrlMac, 6) != 0) {
        memcpy(ctrlMac, mac, 6);
        haveCtrl = true;
        char ms[18];
        Link::macToString(mac, ms);
        Serial.printf("controller found: %s\n", ms);
    }
    lastCmd = p;
    lastCmdAt = now;
    if (cmdTimeout) { cmdTimeout = false; Serial.println("commands flowing"); }
}

static void sendTelemetry() {
    if (!haveCtrl) return;
    TelemetryPacket t;
    t.magic   = TELEMETRY_MAGIC;
    t.seqAck  = lastCmd.seq;
    t.left    = (int8_t)curL;
    t.right   = (int8_t)curR;
    t.flags   = (uint8_t)((curL || curR ? TLM_MOTORS_ON : 0) | (bonnetOk ? TLM_BONNET_OK : 0) |
                          (cmdTimeout ? TLM_CMD_TIMEOUT : 0));
    t.vbat_mv = 0;
    radio.send(ctrlMac, &t, sizeof t);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Rover ===");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, ROVER_I2C_CLOCK_HZ);
    bonnetOk = bonnet.begin(BONNET_PWM_HZ);
    Serial.println(bonnetOk ? "motor bonnet ready" : "motor bonnet NOT found at 0x60 (link still works; motors disabled)");

    if (!radio.begin()) {
        while (true) { Serial.println("ESP-NOW init failed"); delay(2000); }
    }
    Serial.printf("radio up, MAC %s. Waiting for controller...\n", radio.localMac().c_str());
}

void loop() {
    static uint32_t lastTick = 0, lastTlm = 0, lastStatus = 0;
    const uint32_t now = millis();

    handleCommands(now);

    if (now - lastTick < ROVER_TICK_MS) return;
    lastTick = now;

    if (!cmdTimeout && now - lastCmdAt > CMD_TIMEOUT_MS) {
        cmdTimeout = true;
        Serial.println("command timeout -> stop");
    }

    const bool estop = lastCmd.flags & CTRL_ESTOP;
    const bool drive = !cmdTimeout && !estop && (lastCmd.flags & CTRL_ARMED);
    if (drive) mix(lastCmd.throttle, lastCmd.steer, targetL, targetR);
    else       targetL = targetR = 0;

    if (estop) { curL = curR = 0; }                 // immediate, no slew
    else       { curL = slew(curL, targetL); curR = slew(curR, targetR); }
    applyMotors();

    if (now - lastTlm >= TELEMETRY_PERIOD_MS) { lastTlm = now; sendTelemetry(); }

    if (now - lastStatus >= 500) {
        lastStatus = now;
        Serial.printf("[%7u ms] %s thr %+4d str %+4d -> L%+4d R%+4d %s%s\n",
                      (unsigned)now, haveCtrl ? (cmdTimeout ? "TIMEOUT " : "linked  ") : "no ctrl ",
                      lastCmd.throttle, lastCmd.steer, curL, curR,
                      (lastCmd.flags & CTRL_ARMED) ? "armed" : "disarmed",
                      bonnetOk ? "" : " [no bonnet]");
    }
}
