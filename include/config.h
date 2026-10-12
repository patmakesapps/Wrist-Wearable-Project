#pragma once
#include <stdint.h>
#include <sdkconfig.h>   // CONFIG_IDF_TARGET_* for per-board pins

// =====================================================================
//  Shared
// =====================================================================
constexpr uint32_t SERIAL_BAUD = 115200;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// Waveshare ESP32-S3-LCD-1.47: GPIO21 feeds the TF card and 38..48 are LCD/RGB, so I2C
// goes on two free header pins instead.
constexpr int PIN_I2C_SDA = 4;    // rover: bonnet SDA (bonnet header pin 3)
constexpr int PIN_I2C_SCL = 5;    // rover: bonnet SCL (bonnet header pin 5)
#else
// Classic ESP32 DevKit
constexpr int PIN_I2C_SDA = 21;   // controller: MPU6050 SDA   rover: bonnet SDA (bonnet header pin 3)
constexpr int PIN_I2C_SCL = 22;   // controller: MPU6050 SCL   rover: bonnet SCL (bonnet header pin 5)
#endif

// =====================================================================
//  Controller (the wearable: ESP32 + MPU6050 + vibration motor)
// =====================================================================
constexpr int      PIN_MOTOR         = 25;      // vibration motor module IN
constexpr uint32_t I2C_CLOCK_HZ      = 400000;
constexpr uint8_t  MPU6050_ADDR      = 0x68;    // AD0 floating/low
constexpr uint32_t MOTOR_PWM_FREQ_HZ = 5000;
constexpr uint8_t  MOTOR_PWM_BITS    = 8;       // levels 0..255
constexpr uint32_t SAMPLE_PERIOD_MS  = 10;      // 100 Hz IMU polling

// Tilt -> drive mapping
constexpr float    TILT_DEADZONE_DEG   = 8.0f;  // below this: zero output
constexpr float    TILT_FULL_DEG       = 35.0f; // at this: 100 %
constexpr float    PITCH_SIGN          = 1.0f;  // flip to -1 if forward/back is reversed
constexpr float    ROLL_SIGN           = 1.0f;  // flip to -1 if left/right is reversed
constexpr float    TILT_FILTER_ALPHA   = 0.2f;  // EMA on tilt, per 10 ms sample
constexpr uint32_t CONTROL_PERIOD_MS   = 50;    // 20 Hz command rate

// Arming
constexpr float    LEVEL_TOLERANCE_DEG = 8.0f;  // "level" = both axes inside this
constexpr uint32_t ARM_HOLD_MS         = 1000;  // hold level this long to arm
constexpr uint32_t ARM_LOCKOUT_MS      = 1500;  // no re-arm this soon after a disarm
constexpr float    ESTOP_SHAKE_G       = 1.5f;  // accel deviation that disarms (sharp shake)
constexpr uint32_t LINK_TIMEOUT_MS     = 600;   // no telemetry for this long = link lost

// Stand-alone motion->buzz demo (src/standalone)
constexpr float    ACCEL_MOVE_THRESHOLD_G    = 0.35f;
constexpr float    GYRO_ROTATE_THRESHOLD_DPS = 150.0f;
constexpr uint8_t  MOTION_MIN_SAMPLES        = 2;
constexpr uint32_t MOTION_COOLDOWN_MS        = 1000;

// =====================================================================
//  Rover (spare ESP32 + Adafruit Motor Bonnet + 2x N20)
// =====================================================================
constexpr uint8_t  BONNET_ADDR          = 0x60;
constexpr uint16_t BONNET_PWM_HZ        = 1600;
constexpr uint32_t ROVER_I2C_CLOCK_HZ   = 100000; // bonnet has no pull-ups; ESP32 internal ones only
constexpr uint8_t  MOTOR_LEFT           = 1;      // bonnet terminal M1 (one end of the board)
constexpr uint8_t  MOTOR_RIGHT          = 4;      // bonnet terminal M4 (the other end)
constexpr int      MOTOR_LEFT_SIGN      = 1;      // flip to -1 if that wheel runs backwards
constexpr int      MOTOR_RIGHT_SIGN     = 1;
constexpr uint8_t  MOTOR_MIN_PERCENT    = 25;     // N20s stall below this; remap 1..100 onto 25..100
constexpr uint8_t  MOTOR_SLEW_PER_TICK  = 8;      // max % change per tick (protects the gearboxes)
constexpr uint32_t ROVER_TICK_MS        = 20;     // 50 Hz control loop
constexpr uint32_t CMD_TIMEOUT_MS       = 300;    // no command for this long = stop
constexpr uint32_t TELEMETRY_PERIOD_MS  = 100;    // 10 Hz back to the controller
