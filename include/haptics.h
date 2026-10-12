#pragma once
#include <Arduino.h>

// One step of a vibration pattern: hold "level" (0..255) for "ms" milliseconds.
struct HapticStep { uint16_t ms; uint8_t level; };

struct HapticPattern {
    const HapticStep *steps;
    uint8_t count;
    const char *name;
};

namespace Patterns {
extern const HapticPattern TAP;
extern const HapticPattern DOUBLE_TAP;
extern const HapticPattern LONG_BUZZ;
extern const HapticPattern RAMP_UP;
}

// PWM-driven vibration motor with a non-blocking pattern player.
class Haptics {
public:
    bool begin(int pin, uint32_t freqHz, uint8_t bits);
    void set(uint8_t level);                      // raw PWM level, cancels any pattern
    void off();
    void pulse(uint16_t ms, uint8_t level = 255); // blocking, for bring-up tests
    void play(const HapticPattern &p);            // non-blocking; call update() from loop()
    void update();
    bool busy() const { return _active; }

private:
    int _pin = -1;
    const HapticPattern *_pat = nullptr;
    uint8_t  _idx = 0;
    uint32_t _stepStart = 0;
    bool _active = false;
};
