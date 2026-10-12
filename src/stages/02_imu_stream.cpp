// Stage 2: initialise the MPU6050 and stream live readings at 20 Hz.
// Lying flat and still you should see az ~ +1.00 g, |a| ~ 1.00, gyro ~ 0 after calibration.
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "imu.h"

static Mpu6050 imu(MPU6050_ADDR);

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Stage 2: MPU6050 live stream ===");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);

    uint8_t who = 0;
    if (!imu.probe(who)) {
        Serial.printf("MPU6050 not responding at 0x%02X (I2C error %u). Run stage1.\n", MPU6050_ADDR, imu.lastError());
        while (true) delay(1000);
    }
    Serial.printf("WHO_AM_I = 0x%02X\n", who);

    if (!imu.begin(Mpu6050::AccelRange::G4, Mpu6050::GyroRange::DPS500)) {
        Serial.printf("MPU6050 init failed (I2C error %u)\n", imu.lastError());
        while (true) delay(1000);
    }
    Serial.println("Init OK: +/-4 g, +/-500 dps, DLPF 44 Hz, 100 Hz");

    Serial.println("Hold still ~1 s: calibrating gyro offset...");
    Serial.println(imu.calibrateGyro(200) ? "Gyro calibrated." : "Gyro calibration failed (read errors).");

    Serial.println();
    Serial.println("   t_ms |    ax     ay     az  [g] |     gx      gy      gz [dps] |  |a|  pitch   roll | temp");
}

void loop() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 50) return;      // 20 Hz print rate
    last = now;

    ImuSample s;
    if (!imu.read(s)) {
        Serial.printf("read error %u\n", imu.lastError());
        return;
    }
    float pitch, roll;
    accelToTilt(s, pitch, roll);
    Serial.printf("%7u | %6.2f %6.2f %6.2f     | %7.1f %7.1f %7.1f       | %4.2f %6.1f %6.1f | %4.1f C\n",
                  (unsigned)s.t_ms, s.ax, s.ay, s.az, s.gx, s.gy, s.gz,
                  accelMagnitude(s), pitch, roll, s.tempC);
}
