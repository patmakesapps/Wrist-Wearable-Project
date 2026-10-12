#pragma once
#include <Arduino.h>

// One IMU reading in physical units.
struct ImuSample {
    float ax, ay, az;   // g
    float gx, gy, gz;   // deg/s (gyro bias removed after calibrateGyro)
    float tempC;        // die temperature
    uint32_t t_ms;      // millis() at read time
};

// Minimal register-level MPU6050 driver (no external library).
class Mpu6050 {
public:
    enum class AccelRange : uint8_t { G2 = 0, G4 = 1, G8 = 2, G16 = 3 };
    enum class GyroRange  : uint8_t { DPS250 = 0, DPS500 = 1, DPS1000 = 2, DPS2000 = 3 };

    explicit Mpu6050(uint8_t addr = 0x68) : _addr(addr) {}

    // True if the device ACKs; whoAmI receives register 0x75 (0x68 on a genuine MPU6050).
    bool probe(uint8_t &whoAmI);
    // Reset, wake, configure filter/rate/ranges. Wire.begin() must already have been called.
    bool begin(AccelRange ar = AccelRange::G4, GyroRange gr = GyroRange::DPS500);
    // Read accel/temp/gyro in one burst.
    bool read(ImuSample &out);
    // Average N readings while still to remove gyro offset.
    bool calibrateGyro(uint16_t samples = 200);

    uint8_t lastError() const { return _err; }

private:
    bool writeReg(uint8_t reg, uint8_t val);
    bool readRegs(uint8_t reg, uint8_t *buf, size_t len);

    uint8_t _addr;
    uint8_t _err = 0;
    float _accelLsbPerG  = 8192.0f;
    float _gyroLsbPerDps = 65.5f;
    float _gyroBias[3]   = {0, 0, 0};
};

// ---- helpers ----
float accelMagnitude(const ImuSample &s);                        // g
float gyroMagnitude(const ImuSample &s);                         // deg/s
void  accelToTilt(const ImuSample &s, float &pitchDeg, float &rollDeg);
