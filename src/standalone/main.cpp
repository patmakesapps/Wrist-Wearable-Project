// Full firmware: read the MPU6050 at 100 Hz, detect movement/rotation above a threshold,
// buzz the motor with a pattern, enforce a cooldown. Serial commands let you tune live.
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "imu.h"
#include "haptics.h"
#include "motion.h"

static Mpu6050        imu(MPU6050_ADDR);
static Haptics        haptics;
static MotionDetector detector(ACCEL_MOVE_THRESHOLD_G, GYRO_ROTATE_THRESHOLD_DPS,
                               MOTION_MIN_SAMPLES, MOTION_COOLDOWN_MS);

static bool     streamEnabled = true;    // periodic status line
static uint32_t eventCount    = 0;

static void printHelp() {
    Serial.println("Commands:  b=buzz test  1..4=play pattern  s=toggle status stream");
    Serial.println("           +/- accel threshold (0.05 g)   [/] gyro threshold (25 dps)");
    Serial.println("           c=recalibrate gyro (hold still)   ?=help");
}

static void printThresholds() {
    Serial.printf("thresholds: accel %.2f g, gyro %.0f dps, cooldown %u ms\n",
                  detector.accelThreshold(), detector.gyroThreshold(), (unsigned)MOTION_COOLDOWN_MS);
}

static void halt(const char *msg) {
    while (true) {
        Serial.println(msg);
        delay(2000);
    }
}

static void handleSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        switch (c) {
            case 'b': haptics.play(Patterns::LONG_BUZZ);  Serial.println("buzz"); break;
            case '1': haptics.play(Patterns::TAP);        Serial.println("tap"); break;
            case '2': haptics.play(Patterns::DOUBLE_TAP); Serial.println("double-tap"); break;
            case '3': haptics.play(Patterns::LONG_BUZZ);  Serial.println("long-buzz"); break;
            case '4': haptics.play(Patterns::RAMP_UP);    Serial.println("ramp-up"); break;
            case 's': streamEnabled = !streamEnabled;
                      Serial.printf("status stream %s\n", streamEnabled ? "on" : "off"); break;
            case '+': detector.setAccelThreshold(detector.accelThreshold() + 0.05f); printThresholds(); break;
            case '-': detector.setAccelThreshold(detector.accelThreshold() - 0.05f); printThresholds(); break;
            case ']': detector.setGyroThreshold(detector.gyroThreshold() + 25.f);    printThresholds(); break;
            case '[': detector.setGyroThreshold(detector.gyroThreshold() - 25.f);    printThresholds(); break;
            case 'c': Serial.println("hold still: recalibrating gyro...");
                      Serial.println(imu.calibrateGyro(200) ? "done" : "failed"); break;
            case '?': printHelp(); printThresholds(); break;
            default: break;   // ignore CR/LF etc.
        }
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Wrist Haptic Wearable ===");

    if (!haptics.begin(PIN_MOTOR, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS)) halt("motor PWM init failed");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
    uint8_t who = 0;
    if (!imu.probe(who)) halt("MPU6050 not found at 0x68 - check wiring (run stage1_i2c_scan)");
    Serial.printf("MPU6050 WHO_AM_I = 0x%02X %s\n", who, who == 0x68 ? "(ok)" : "(unexpected id, continuing)");
    if (!imu.begin(Mpu6050::AccelRange::G4, Mpu6050::GyroRange::DPS500)) halt("MPU6050 init failed");

    Serial.println("hold still: calibrating gyro...");
    imu.calibrateGyro(200);

    printHelp();
    printThresholds();
    haptics.play(Patterns::DOUBLE_TAP);   // "ready" signal
}

void loop() {
    static uint32_t lastSample = 0, lastStatus = 0;
    const uint32_t now = millis();

    haptics.update();
    handleSerial();

    if (now - lastSample < SAMPLE_PERIOD_MS) return;
    lastSample = now;

    ImuSample s;
    if (!imu.read(s)) {
        Serial.printf("IMU read error %u\n", imu.lastError());
        return;
    }

    MotionEvent ev = detector.feed(s);
    if (ev != MotionEvent::None) {
        ++eventCount;
        Serial.printf("[%7u ms] EVENT #%u %-6s  accel-dev %.2f g  gyro %.0f dps\n",
                      (unsigned)now, (unsigned)eventCount, motionEventName(ev),
                      detector.accelDeviation(), detector.gyroMag());
        haptics.play(ev == MotionEvent::Rotate ? Patterns::DOUBLE_TAP : Patterns::TAP);
    }

    if (streamEnabled && now - lastStatus >= 500) {
        lastStatus = now;
        Serial.printf("[%7u ms] accel-dev %.2f g  gyro %5.0f dps  %s\n",
                      (unsigned)now, detector.accelDeviation(), detector.gyroMag(),
                      detector.inCooldown() ? "(cooldown)" : "");
    }
}
