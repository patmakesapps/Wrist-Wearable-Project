// Controller (wearable): tilt of the hand -> throttle/steer, sent to the rover over
// ESP-NOW at 20 Hz. Hold level 1 s to arm, sharp shake to disarm. Haptics confirm arm /
// disarm and warn when the rover stops answering.
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "imu.h"
#include "haptics.h"
#include "link.h"
#include "packets.h"

static Mpu6050 imu(MPU6050_ADDR);
static Haptics haptics;
static Link        radio;

// tilt
static float pitchF = 0, rollF = 0;          // filtered, degrees
static float pitchZero = 0, rollZero = 0;    // neutral offset (press 'z' while holding neutral)
static float accelDev = 0;
static bool  tiltInit = false;

// arming
static bool     armed = false;
static bool     wasLevel = false;
static uint32_t levelSince = 0;
static uint32_t disarmedAt = 0;

// link
static uint16_t seq = 0;
static uint8_t  roverMac[6] = {0};
static bool     haveRover = false;
static bool     linkUp = false;
static uint32_t lastTelemetryAt = 0;
static uint32_t lastLinkBuzz = 0;
static TelemetryPacket lastTlm = {};

static bool   streamEnabled = true;
static int8_t throttle = 0, steer = 0;

static void printHelp() {
    Serial.println("Commands: a=arm  d=disarm  z=zero tilt (hold neutral)  s=toggle status stream");
    Serial.println("          b=buzz  c=recalibrate gyro (hold still)  ?=help");
    Serial.println("Gestures: hold level 1 s = arm   sharp shake = disarm (e-stop)");
}

static int8_t tiltToAxis(float deg) {
    const float mag = fabsf(deg);
    if (mag < TILT_DEADZONE_DEG) return 0;
    float n = (mag - TILT_DEADZONE_DEG) / (TILT_FULL_DEG - TILT_DEADZONE_DEG);
    if (n > 1.0f) n = 1.0f;
    const int v = (int)lroundf(n * 100.0f);
    return (int8_t)(deg < 0 ? -v : v);
}

static void sendControl(uint8_t extraFlags) {
    ControlPacket p;
    p.magic    = CONTROL_MAGIC;
    p.seq      = seq++;
    p.throttle = armed ? throttle : (int8_t)0;
    p.steer    = armed ? steer : (int8_t)0;
    p.flags    = (uint8_t)((armed ? CTRL_ARMED : 0) | extraFlags);
    if (haveRover) radio.send(roverMac, &p, sizeof p);
    else           radio.broadcast(&p, sizeof p);
}

static void setArmed(bool on, const char *why) {
    if (armed == on) return;
    armed = on;
    Serial.printf("[%7u ms] %s (%s)\n", (unsigned)millis(), on ? "ARMED" : "DISARMED", why);
    haptics.play(on ? Patterns::TAP : Patterns::DOUBLE_TAP);
    if (!on) {
        disarmedAt = millis();
        wasLevel = false;
        sendControl(CTRL_ESTOP);            // tell the rover right now, don't wait for the next tick
    }
}

static void handleSerial() {
    while (Serial.available()) {
        switch ((char)Serial.read()) {
            case 'a': setArmed(true, "serial"); break;
            case 'd': setArmed(false, "serial"); break;
            case 'z': pitchZero = pitchF; rollZero = rollF;
                      Serial.printf("tilt zeroed at pitch %.1f roll %.1f\n", pitchZero, rollZero); break;
            case 's': streamEnabled = !streamEnabled;
                      Serial.printf("status stream %s\n", streamEnabled ? "on" : "off"); break;
            case 'b': haptics.play(Patterns::LONG_BUZZ); break;
            case 'c': Serial.println("hold still: recalibrating gyro...");
                      Serial.println(imu.calibrateGyro(200) ? "done" : "failed"); break;
            case '?': printHelp(); break;
            default: break;
        }
    }
}

static void handleTelemetry(uint32_t now) {
    uint8_t buf[64];
    size_t len = 0;
    uint8_t mac[6];
    if (!radio.receive(buf, sizeof buf, len, mac)) return;
    if (len != sizeof(TelemetryPacket)) return;
    TelemetryPacket t;
    memcpy(&t, buf, sizeof t);
    if (t.magic != TELEMETRY_MAGIC) return;

    if (!haveRover || memcmp(mac, roverMac, 6) != 0) {
        memcpy(roverMac, mac, 6);
        haveRover = true;
        char ms[18];
        Link::macToString(mac, ms);
        Serial.printf("rover found: %s\n", ms);
    }
    lastTelemetryAt = now;
    lastTlm = t;
    if (!linkUp) {
        linkUp = true;
        Serial.println("link up");
        if (armed) haptics.play(Patterns::TAP);
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Wrist Controller ===");

    if (!haptics.begin(PIN_MOTOR, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS)) Serial.println("motor PWM init failed");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
    uint8_t who = 0;
    if (!imu.probe(who)) {
        while (true) { Serial.println("MPU6050 not found at 0x68 (run stage1_i2c_scan)"); delay(2000); }
    }
    Serial.printf("MPU6050 WHO_AM_I = 0x%02X\n", who);
    if (!imu.begin(Mpu6050::AccelRange::G4, Mpu6050::GyroRange::DPS500)) {
        while (true) { Serial.println("MPU6050 init failed"); delay(2000); }
    }
    Serial.println("hold still: calibrating gyro...");
    imu.calibrateGyro(200);

    if (!radio.begin()) {
        while (true) { Serial.println("ESP-NOW init failed"); delay(2000); }
    }
    Serial.printf("radio up, MAC %s. Waiting for rover telemetry...\n", radio.localMac().c_str());

    printHelp();
    haptics.play(Patterns::DOUBLE_TAP);
}

void loop() {
    static uint32_t lastSample = 0, lastSend = 0, lastStatus = 0;
    const uint32_t now = millis();

    haptics.update();
    handleSerial();
    handleTelemetry(now);

    if (linkUp && now - lastTelemetryAt > LINK_TIMEOUT_MS) {
        linkUp = false;
        Serial.println("link lost");
    }
    if (armed && haveRover && !linkUp && now - lastLinkBuzz > 2000) {
        lastLinkBuzz = now;
        haptics.play(Patterns::LONG_BUZZ);
    }

    if (now - lastSample >= SAMPLE_PERIOD_MS) {
        lastSample = now;
        ImuSample s;
        if (imu.read(s)) {
            float pitch, roll;
            accelToTilt(s, pitch, roll);
            if (!tiltInit) { pitchF = pitch; rollF = roll; tiltInit = true; }
            pitchF += TILT_FILTER_ALPHA * (pitch - pitchF);
            rollF  += TILT_FILTER_ALPHA * (roll - rollF);
            accelDev = fabsf(accelMagnitude(s) - 1.0f);

            const float p = (pitchF - pitchZero) * PITCH_SIGN;
            const float r = (rollF - rollZero) * ROLL_SIGN;
            throttle = tiltToAxis(p);
            steer    = tiltToAxis(r);

            if (armed && accelDev > ESTOP_SHAKE_G) setArmed(false, "shake");

            const bool level = fabsf(p) < LEVEL_TOLERANCE_DEG && fabsf(r) < LEVEL_TOLERANCE_DEG && accelDev < 0.1f;
            if (!armed && now - disarmedAt > ARM_LOCKOUT_MS) {
                if (level && !wasLevel) levelSince = now;
                if (level && wasLevel && now - levelSince >= ARM_HOLD_MS) setArmed(true, "held level");
            }
            wasLevel = level;
        }
    }

    if (now - lastSend >= CONTROL_PERIOD_MS) {
        lastSend = now;
        sendControl(0);
    }

    if (streamEnabled && now - lastStatus >= 500) {
        lastStatus = now;
        Serial.printf("[%7u ms] %-8s pitch %+6.1f roll %+6.1f -> thr %+4d str %+4d | rover %s L%+4d R%+4d%s\n",
                      (unsigned)now, armed ? "ARMED" : "disarmed",
                      pitchF - pitchZero, rollF - rollZero, throttle, steer,
                      haveRover ? (linkUp ? "up  " : "LOST") : "none",
                      lastTlm.left, lastTlm.right,
                      (lastTlm.flags & TLM_CMD_TIMEOUT) ? " (rover timeout)" : "");
    }
}
