#pragma once
#include <stdint.h>

// Wire format shared by controller and rover (ESP-NOW payloads, max 250 bytes).

constexpr uint32_t CONTROL_MAGIC   = 0x54535257; // "WRST"
constexpr uint32_t TELEMETRY_MAGIC = 0x52564F52; // "ROVR"
constexpr uint32_t HEARTBEAT_MAGIC = 0x21544248; // "HBT!"

enum ControlFlags : uint8_t {
    CTRL_ARMED = 1 << 0,
    CTRL_ESTOP = 1 << 1,
};

enum TelemetryFlags : uint8_t {
    TLM_MOTORS_ON   = 1 << 0,  // at least one motor is being driven
    TLM_BONNET_OK   = 1 << 1,  // motor bonnet initialised
    TLM_CMD_TIMEOUT = 1 << 2,  // rover stopped itself: no command recently
};

struct __attribute__((packed)) ControlPacket {
    uint32_t magic;
    uint16_t seq;
    int8_t   throttle;   // -100..100, + = forward
    int8_t   steer;      // -100..100, + = right
    uint8_t  flags;      // ControlFlags
};

struct __attribute__((packed)) TelemetryPacket {
    uint32_t magic;
    uint16_t seqAck;     // last control seq seen
    int8_t   left;       // applied motor percent
    int8_t   right;
    uint8_t  flags;      // TelemetryFlags
    uint16_t vbat_mv;    // 0 = not measured
};

struct __attribute__((packed)) HeartbeatPacket {
    uint32_t magic;
    uint16_t seq;
    uint32_t uptime_ms;
};
