#include "motion.h"
#include <math.h>

const char *motionEventName(MotionEvent e) {
    switch (e) {
        case MotionEvent::Move:   return "MOVE";
        case MotionEvent::Rotate: return "ROTATE";
        default:                  return "none";
    }
}

MotionEvent MotionDetector::feed(const ImuSample &s) {
    _accelDev = fabsf(accelMagnitude(s) - 1.0f);
    _gyroMag  = gyroMagnitude(s);

    const uint32_t now = s.t_ms;
    if (!_armed) {
        if (now - _lastEvent < _cooldown) return MotionEvent::None;
        _armed = true;
        _overCount = 0;
    }

    const float accelRatio = _accelDev / _accelThr;
    const float gyroRatio  = _gyroMag  / _gyroThr;
    if (accelRatio < 1.0f && gyroRatio < 1.0f) { _overCount = 0; return MotionEvent::None; }

    if (++_overCount < _minSamples) return MotionEvent::None;

    _overCount = 0;
    _armed = false;
    _lastEvent = now;
    return gyroRatio >= accelRatio ? MotionEvent::Rotate : MotionEvent::Move;
}
