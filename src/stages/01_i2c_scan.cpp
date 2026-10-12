// Stage 1: prove the I2C wiring. Scans the bus and reads MPU6050 WHO_AM_I.
// Expected: "device at 0x68" and "WHO_AM_I = 0x68 -> MPU6050 confirmed".
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Stage 1: I2C bus scan + MPU6050 WHO_AM_I ===");

    // Wiring sanity check before the I2C peripheral owns the pins.
    // The GY-521 has 2.2k pull-ups to its VCC, which beat the ESP32 internal pull-down,
    // so a powered + connected module reads 1/1. 0 means no power / no connection.
    pinMode(PIN_I2C_SDA, INPUT_PULLDOWN);
    pinMode(PIN_I2C_SCL, INPUT_PULLDOWN);
    delay(5);
    int sda = digitalRead(PIN_I2C_SDA);
    int scl = digitalRead(PIN_I2C_SCL);
    Serial.printf("Idle line levels: SDA=%d SCL=%d  (expect 1/1; 0 => check VCC/GND or that wire)\n", sda, scl);

    if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000)) {
        Serial.println("Wire.begin() failed");
    }
}

void loop() {
    Serial.println("Scanning 0x08..0x77 ...");
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  device at 0x%02X%s\n", a, a == MPU6050_ADDR ? "   <-- MPU6050 (expected)" : "");
            ++found;
        }
    }
    if (found == 0) {
        Serial.println("  no devices found. Check VCC->3V3, GND->GND, SDA->GPIO21, SCL->GPIO22.");
    }

    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x75);                              // WHO_AM_I
    uint8_t err = Wire.endTransmission(false);
    if (err == 0 && Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)1) == 1) {
        uint8_t who = Wire.read();
        const char *verdict = (who == 0x68) ? "MPU6050 confirmed"
                                            : "unexpected ID (clone? MPU6500=0x70, MPU9250=0x71)";
        Serial.printf("WHO_AM_I (0x75) = 0x%02X -> %s\n", who, verdict);
    } else {
        Serial.printf("WHO_AM_I read failed (I2C error %u)\n", err);
    }
    Serial.println();
    delay(2000);
}
