#include "bonnet.h"
#include <Wire.h>

namespace {
constexpr uint8_t REG_MODE1     = 0x00;
constexpr uint8_t REG_MODE2     = 0x01;
constexpr uint8_t REG_LED0_ON_L = 0x06;
constexpr uint8_t REG_PRESCALE  = 0xFE;

constexpr uint8_t MODE1_RESTART = 0x80;
constexpr uint8_t MODE1_AI      = 0x20;   // register auto-increment
constexpr uint8_t MODE1_SLEEP   = 0x10;
constexpr uint8_t MODE1_ALLCALL = 0x01;
constexpr uint8_t MODE2_OUTDRV  = 0x04;   // totem-pole outputs

struct MotorPins { uint8_t pwm, in1, in2; };
// PCA9685 channels per motor terminal (Motor Shield V2 / Motor HAT / Motor Bonnet).
constexpr MotorPins MOTOR_MAP[4] = {
    {8, 10, 9},    // M1
    {13, 11, 12},  // M2
    {2, 4, 3},     // M3
    {7, 5, 6},     // M4
};
}

bool MotorBonnet::write8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    _err = Wire.endTransmission();
    return _err == 0;
}

bool MotorBonnet::read8(uint8_t reg, uint8_t &val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    _err = Wire.endTransmission(false);
    if (_err != 0) return false;
    if (Wire.requestFrom((uint8_t)_addr, (uint8_t)1) != 1) { _err = 0xFF; return false; }
    val = Wire.read();
    return true;
}

bool MotorBonnet::probe() {
    uint8_t v = 0;
    return read8(REG_MODE1, v);
}

bool MotorBonnet::begin(uint16_t pwmHz) {
    if (!probe()) return false;
    if (!write8(REG_MODE1, MODE1_AI | MODE1_ALLCALL)) return false;   // wake, auto-increment on
    delay(5);
    if (!write8(REG_MODE2, MODE2_OUTDRV)) return false;

    // prescale = round(25 MHz / (4096 * f)) - 1, minimum 3
    float ps = 25000000.0f / (4096.0f * (float)pwmHz) - 1.0f;
    int prescale = (int)(ps + 0.5f);
    if (prescale < 3) prescale = 3;
    if (prescale > 255) prescale = 255;

    if (!write8(REG_MODE1, MODE1_AI | MODE1_ALLCALL | MODE1_SLEEP)) return false;  // must sleep to set prescale
    if (!write8(REG_PRESCALE, (uint8_t)prescale)) return false;
    if (!write8(REG_MODE1, MODE1_AI | MODE1_ALLCALL)) return false;
    delay(5);
    if (!write8(REG_MODE1, MODE1_AI | MODE1_ALLCALL | MODE1_RESTART)) return false;

    stopAll();
    return true;
}

bool MotorBonnet::setChannel(uint8_t ch, uint16_t on, uint16_t off) {
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(REG_LED0_ON_L + 4 * ch));
    Wire.write((uint8_t)(on & 0xFF));
    Wire.write((uint8_t)(on >> 8));
    Wire.write((uint8_t)(off & 0xFF));
    Wire.write((uint8_t)(off >> 8));
    _err = Wire.endTransmission();
    return _err == 0;
}

// Full-on is ON_H bit 4 (4096); full-off is OFF_H bit 4.
bool MotorBonnet::setChannelLevel(uint8_t ch, bool high) {
    return high ? setChannel(ch, 4096, 0) : setChannel(ch, 0, 4096);
}

bool MotorBonnet::setMotor(uint8_t motor, int percent) {
    if (motor < 1 || motor > 4) return false;
    const MotorPins &p = MOTOR_MAP[motor - 1];
    if (percent > 100) percent = 100;
    if (percent < -100) percent = -100;

    bool ok = true;
    if (percent == 0) {                       // coast: both inputs low, PWM off
        ok &= setChannelLevel(p.in1, false);
        ok &= setChannelLevel(p.in2, false);
        ok &= setChannel(p.pwm, 0, 4096);
        return ok;
    }
    const bool fwd = percent > 0;
    ok &= setChannelLevel(p.in1, fwd);
    ok &= setChannelLevel(p.in2, !fwd);
    const int mag = fwd ? percent : -percent;
    const uint16_t duty = (uint16_t)(mag * 4095 / 100);
    ok &= setChannel(p.pwm, 0, duty);
    return ok;
}

bool MotorBonnet::brake(uint8_t motor) {
    if (motor < 1 || motor > 4) return false;
    const MotorPins &p = MOTOR_MAP[motor - 1];
    bool ok = setChannelLevel(p.in1, true);
    ok &= setChannelLevel(p.in2, true);
    ok &= setChannel(p.pwm, 4096, 0);
    return ok;
}

void MotorBonnet::stopAll() {
    for (uint8_t m = 1; m <= 4; ++m) setMotor(m, 0);
}
