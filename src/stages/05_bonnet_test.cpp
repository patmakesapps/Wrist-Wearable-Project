// Stage 5: prove the motor bonnet wiring (rover ESP32 only).
// Scans I2C (expect 0x60, plus 0x70 which is the PCA9685 all-call address), initialises
// the bonnet, then runs the left and right motors (MOTOR_LEFT / MOTOR_RIGHT in config.h)
// forward/reverse and a ramp, printing each step.
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "bonnet.h"

static MotorBonnet bonnet(BONNET_ADDR);
static bool ready = false;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Stage 5: motor bonnet test ===");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, ROVER_I2C_CLOCK_HZ);

    Serial.println("Scanning I2C 0x08..0x77 ...");
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            const char *note = a == BONNET_ADDR ? "   <-- motor bonnet (PCA9685)"
                             : a == 0x70        ? "   (PCA9685 all-call, expected)" : "";
            Serial.printf("  device at 0x%02X%s\n", a, note);
            ++found;
        }
    }
    if (found == 0) Serial.println("  nothing found. Check 3V3->pin1, GND->pin6, SDA->pin3, SCL->pin5.");

    if (!bonnet.begin(BONNET_PWM_HZ)) {
        Serial.printf("bonnet init failed (I2C error %u)\n", bonnet.lastError());
        return;
    }
    ready = true;
    Serial.printf("bonnet ready: PWM %u Hz. Motor power must be on the bonnet's +/- terminal.\n", BONNET_PWM_HZ);
}

void loop() {
    if (!ready) {
        Serial.println("bonnet not initialised; fix wiring and reset");
        delay(3000);
        return;
    }
    static const struct { const char *side; uint8_t m; } motors[] = {
        {"left", MOTOR_LEFT}, {"right", MOTOR_RIGHT}};
    for (const auto &mo : motors) {
        Serial.printf("%s (M%u) forward 60%%\n", mo.side, mo.m);  bonnet.setMotor(mo.m, 60);  delay(1000);
        Serial.printf("%s (M%u) stop\n", mo.side, mo.m);           bonnet.setMotor(mo.m, 0);   delay(500);
        Serial.printf("%s (M%u) reverse 60%%\n", mo.side, mo.m);  bonnet.setMotor(mo.m, -60); delay(1000);
        Serial.printf("%s (M%u) stop\n", mo.side, mo.m);           bonnet.setMotor(mo.m, 0);   delay(500);
    }
    Serial.println("both forward, ramp 30 -> 100 %");
    for (int p = 30; p <= 100; p += 10) {
        bonnet.setMotor(MOTOR_LEFT, p);
        bonnet.setMotor(MOTOR_RIGHT, p);
        delay(250);
    }
    bonnet.stopAll();
    Serial.println("--- cycle done ---");
    delay(2000);
}
