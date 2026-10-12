#include "haptics.h"

namespace Patterns {
static const HapticStep tapSteps[]       = {{60, 255}};
static const HapticStep doubleTapSteps[] = {{60, 255}, {80, 0}, {60, 255}};
static const HapticStep longBuzzSteps[]  = {{400, 200}};
static const HapticStep rampSteps[]      = {{80, 70}, {80, 130}, {80, 190}, {140, 255}};

const HapticPattern TAP        = {tapSteps,       1, "tap"};
const HapticPattern DOUBLE_TAP = {doubleTapSteps, 3, "double-tap"};
const HapticPattern LONG_BUZZ  = {longBuzzSteps,  1, "long-buzz"};
const HapticPattern RAMP_UP    = {rampSteps,      4, "ramp-up"};
}

bool Haptics::begin(int pin, uint32_t freqHz, uint8_t bits) {
    _pin = pin;
    if (!ledcAttach((uint8_t)pin, freqHz, bits)) return false;
    off();
    return true;
}

void Haptics::set(uint8_t level) {
    _active = false;
    _pat = nullptr;
    ledcWrite((uint8_t)_pin, level);
}

void Haptics::off() { set(0); }

void Haptics::pulse(uint16_t ms, uint8_t level) {
    set(level);
    delay(ms);
    set(0);
}

void Haptics::play(const HapticPattern &p) {
    if (p.count == 0) { off(); return; }
    ledcWrite((uint8_t)_pin, p.steps[0].level);
    _pat = &p;
    _idx = 0;
    _stepStart = millis();
    _active = true;
}

void Haptics::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now - _stepStart < _pat->steps[_idx].ms) return;
    if (++_idx >= _pat->count) { off(); return; }
    _stepStart = now;
    ledcWrite((uint8_t)_pin, _pat->steps[_idx].level);
}
