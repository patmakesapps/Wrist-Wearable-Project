#pragma once
#include "imu.h"

enum class MotionEvent : uint8_t { None, Move, Rotate };

const char *motionEventName(MotionEvent e);

// Simple threshold detector with debounce and cooldown.
//   Move   : |accel| deviates from 1 g by more than accelThrG
//   Rotate : |gyro| exceeds gyroThrDps
class MotionDetector {
public:
    MotionDetector(float accelThrG, float gyroThrDps, uint8_t minSamples, uint32_t cooldownMs)
        : _accelThr(accelThrG), _gyroThr(gyroThrDps), _minSamples(minSamples), _cooldown(cooldownMs) {}

    MotionEvent feed(const ImuSample &s);

    float accelDeviation() const { return _accelDev; }  // | |a| - 1 g |
    float gyroMag() const        { return _gyroMag; }
    bool  inCooldown() const     { return !_armed; }

    float accelThreshold() const { return _accelThr; }
    float gyroThreshold() const  { return _gyroThr; }
    void  setAccelThreshold(float g)  { _accelThr = g > 0.05f ? g : 0.05f; }
    void  setGyroThreshold(float dps) { _gyroThr = dps > 10.f ? dps : 10.f; }
    void  setCooldown(uint32_t ms)    { _cooldown = ms; }

private:
    float _accelThr, _gyroThr;
    uint8_t _minSamples;
    uint32_t _cooldown;

    float _accelDev = 0, _gyroMag = 0;
    uint8_t _overCount = 0;
    uint32_t _lastEvent = 0;
    bool _armed = true;   // false while in cooldown
};
