#include "imu.h"
#include <Wire.h>
#include <math.h>

namespace {
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_WHO_AM_I     = 0x75;
}

bool Mpu6050::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    _err = Wire.endTransmission();
    return _err == 0;
}

bool Mpu6050::readRegs(uint8_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    _err = Wire.endTransmission(false);               // repeated start
    if (_err != 0) return false;
    uint8_t got = Wire.requestFrom((uint8_t)_addr, (uint8_t)len);
    if (got != len) { _err = 0xFF; return false; }
    for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
    return true;
}

bool Mpu6050::probe(uint8_t &whoAmI) {
    whoAmI = 0;
    return readRegs(REG_WHO_AM_I, &whoAmI, 1);
}

bool Mpu6050::begin(AccelRange ar, GyroRange gr) {
    uint8_t who = 0;
    if (!probe(who)) return false;

    if (!writeReg(REG_PWR_MGMT_1, 0x80)) return false; // device reset
    delay(100);
    if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false; // wake, clock = PLL (gyro X)
    delay(10);
    if (!writeReg(REG_CONFIG, 0x03)) return false;     // DLPF ~44 Hz accel / 42 Hz gyro
    if (!writeReg(REG_SMPLRT_DIV, 9)) return false;    // 1 kHz / (1+9) = 100 Hz
    if (!writeReg(REG_GYRO_CONFIG,  (uint8_t)((uint8_t)gr << 3))) return false;
    if (!writeReg(REG_ACCEL_CONFIG, (uint8_t)((uint8_t)ar << 3))) return false;

    static const float accelLsb[] = {16384.f, 8192.f, 4096.f, 2048.f};
    static const float gyroLsb[]  = {131.f, 65.5f, 32.8f, 16.4f};
    _accelLsbPerG  = accelLsb[(uint8_t)ar];
    _gyroLsbPerDps = gyroLsb[(uint8_t)gr];
    return true;
}

bool Mpu6050::read(ImuSample &s) {
    uint8_t b[14];
    if (!readRegs(REG_ACCEL_XOUT_H, b, sizeof b)) return false;
    auto be16 = [](const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); };
    s.ax    = be16(b + 0)  / _accelLsbPerG;
    s.ay    = be16(b + 2)  / _accelLsbPerG;
    s.az    = be16(b + 4)  / _accelLsbPerG;
    s.tempC = be16(b + 6)  / 340.0f + 36.53f;
    s.gx    = be16(b + 8)  / _gyroLsbPerDps - _gyroBias[0];
    s.gy    = be16(b + 10) / _gyroLsbPerDps - _gyroBias[1];
    s.gz    = be16(b + 12) / _gyroLsbPerDps - _gyroBias[2];
    s.t_ms  = millis();
    return true;
}

bool Mpu6050::calibrateGyro(uint16_t n) {
    _gyroBias[0] = _gyroBias[1] = _gyroBias[2] = 0;
    float sum[3] = {0, 0, 0};
    uint16_t ok = 0;
    ImuSample s;
    for (uint16_t i = 0; i < n; ++i) {
        if (read(s)) { sum[0] += s.gx; sum[1] += s.gy; sum[2] += s.gz; ++ok; }
        delay(5);
    }
    if (ok < n / 2) return false;
    for (int i = 0; i < 3; ++i) _gyroBias[i] = sum[i] / ok;
    return true;
}

float accelMagnitude(const ImuSample &s) {
    return sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
}

float gyroMagnitude(const ImuSample &s) {
    return sqrtf(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);
}

void accelToTilt(const ImuSample &s, float &pitchDeg, float &rollDeg) {
    pitchDeg = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * 180.0f / (float)M_PI;
    rollDeg  = atan2f(s.ay, s.az) * 180.0f / (float)M_PI;
}
