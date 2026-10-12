#pragma once
#include <Arduino.h>

// Adafruit DC + Stepper Motor Bonnet / HAT: a PCA9685 PWM chip driving two TB6612
// H-bridges. Same channel map as the Arduino Motor Shield V2. Register-level, no library.
// Wire.begin() must already have been called.
class MotorBonnet {
public:
    explicit MotorBonnet(uint8_t addr = 0x60) : _addr(addr) {}

    bool probe();                            // device ACKs and MODE1 reads back
    bool begin(uint16_t pwmHz = 1600);       // wake, set PWM frequency, all motors off
    bool setMotor(uint8_t motor, int percent); // motor 1..4 (M1..M4), -100..100, 0 = coast
    bool brake(uint8_t motor);
    void stopAll();
    uint8_t lastError() const { return _err; }

private:
    bool write8(uint8_t reg, uint8_t val);
    bool read8(uint8_t reg, uint8_t &val);
    bool setChannel(uint8_t ch, uint16_t on, uint16_t off);
    bool setChannelLevel(uint8_t ch, bool high);

    uint8_t _addr;
    uint8_t _err = 0;
};
