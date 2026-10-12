// Stage 3: prove the motor wiring and PWM intensity control.
// Boot: three short full-power pulses. Then loops through intensity levels and the
// named patterns, printing what it is doing. Press any key in the monitor for an
// extra pulse.
#include <Arduino.h>
#include "config.h"
#include "haptics.h"

static Haptics haptics;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    Serial.println();
    Serial.println("=== Stage 3: vibration motor test (GPIO25) ===");

    if (!haptics.begin(PIN_MOTOR, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS)) {
        Serial.println("ledcAttach failed");
        while (true) delay(1000);
    }
    Serial.println("PWM attached. 3 x 100 ms pulses at full power...");
    for (int i = 0; i < 3; ++i) {
        haptics.pulse(100, 255);
        delay(200);
    }
    Serial.println("Did you feel/hear three buzzes? If not: check IN->GPIO25, VCC->VIN, GND->GND.");
}

void loop() {
    static const uint8_t levels[] = {64, 128, 192, 255};
    static const HapticPattern *patterns[] = {
        &Patterns::TAP, &Patterns::DOUBLE_TAP, &Patterns::LONG_BUZZ, &Patterns::RAMP_UP};

    // ---- intensity sweep (blocking, simple) ----
    for (uint8_t lvl : levels) {
        Serial.printf("level %3u / 255 for 300 ms\n", lvl);
        haptics.pulse(300, lvl);
        delay(700);
    }

    // ---- named patterns (non-blocking player) ----
    for (const HapticPattern *p : patterns) {
        Serial.printf("pattern: %s\n", p->name);
        haptics.play(*p);
        while (haptics.busy()) { haptics.update(); delay(1); }
        delay(800);
    }

    if (Serial.available()) {
        while (Serial.available()) Serial.read();
        Serial.println("manual pulse");
        haptics.pulse(150, 255);
    }
    Serial.println("--- cycle done ---");
    delay(1500);
}
