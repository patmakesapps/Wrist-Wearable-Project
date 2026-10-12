# Wrist-Controlled Rover

Tilt your hand, the rover drives. Two ESP32s talking over ESP-NOW (direct radio, no router):

- **Controller** (the wearable): ESP32 DevKit + GY-521 MPU6050 + 5 V vibration motor module.
  Hand tilt becomes throttle and steering; the motor gives haptic confirmation and warnings.
- **Rover**: a second ESP32 DevKit + Adafruit DC/Stepper Motor Bonnet + two N20 gear motors,
  inside a 3D-printed hull.

Arduino framework on PlatformIO (pioarduino espressif32 platform, Arduino core 3.x). No external libraries.

## Wiring (all solderless jumper wires)

### Controller

| Device | Pin | ESP32 |
|---|---|---|
| Motor module | IN | GPIO25 |
| Motor module | VCC | VIN (5 V from USB) |
| Motor module | GND | GND |
| MPU6050 | VCC | 3V3 |
| MPU6050 | GND | GND |
| MPU6050 | SDA | GPIO21 |
| MPU6050 | SCL | GPIO22 |

MPU6050 AD0 left open, so the I2C address is 0x68. This particular GY-521 reports WHO_AM_I 0x74 (clone die, works fine).

### Rover

The bonnet is a Pi HAT, but it is just an I2C device. Male-to-male jumpers from the ESP32 into the bonnet's
2x20 female header. Pin 1 is the square-marked corner pin; odd pins are on that row.

| Bonnet | Pin | ESP32 |
|---|---|---|
| 3V3 (logic) | header pin 1 | 3V3 |
| SDA | header pin 3 | GPIO21 |
| SCL | header pin 5 | GPIO22 |
| GND | header pin 6 | GND |
| Motor power + / - | screw terminal | bench: ESP32 VIN / GND. Mobile: 4xAA pack or a USB power bank feeding the ESP32, then VIN -> terminal |
| Left motor | M1 terminals | |
| Right motor | M2 terminals | |

The ESP32's internal pull-ups are the only I2C pull-ups on that bus, so the rover runs I2C at 100 kHz.
If a wheel runs backwards, flip its sign in `config.h`. If the whole thing is mirrored, swap M1/M2.

## Layout

```
platformio.ini             one env per bring-up stage, plus controller / rover / standalone
flash.ps1                  Windows wrapper: .\flash.ps1 <env> [-Port COMx] [-Monitor]
include/config.h           every pin, threshold, rate and sign, in one place
include/packets.h          ESP-NOW wire format (control, telemetry, heartbeat)
include/link.h    src/link.cpp      ESP-NOW wrapper (latest-packet mailbox)
include/imu.h     src/imu.cpp       MPU6050 register driver + tilt helpers
include/haptics.h src/haptics.cpp   PWM vibration motor + pattern player
include/bonnet.h  src/bonnet.cpp    PCA9685/TB6612 motor bonnet driver
include/motion.h  src/motion.cpp    threshold detector (standalone demo only)
src/controller/main.cpp    wearable firmware
src/rover/main.cpp         rover firmware
src/standalone/main.cpp    original motion -> buzz demo, no radio
src/stages/01..05          bring-up tests (see below)
```

## Bring-up, in order

```
.\flash.ps1 stage1_i2c_scan                 controller: SDA/SCL idle high, device at 0x68
.\flash.ps1 stage2_imu_stream               controller: |a| ~1.00 g at rest, tilt follows the board
.\flash.ps1 stage3_motor                    controller: three buzzes, intensity sweep, patterns
.\flash.ps1 stage4_espnow_link              BOTH boards (second one with -Port COMx): each prints
                                            its MAC and the other's heartbeats with RSSI
.\flash.ps1 stage5_bonnet_test -Port COMx   rover: I2C scan shows 0x60 (+0x70 all-call), then
                                            M1/M2 forward, reverse, ramp
.\flash.ps1 controller                      wearable firmware
.\flash.ps1 rover -Port COMx                rover firmware (runs without the bonnet for link tests)
```

Add `-Monitor` to any line to open the serial monitor afterwards (Ctrl+C to exit), or run `pio device monitor --port COMx`.

Stages 1 to 3 were verified on the bench on 2026-10-12. Stages 4 and 5 need the second board plugged in.

## Driving

1. Power both. The controller double-taps on boot; its status line shows `rover none` until the rover answers.
2. Hold your hand level and still for 1 s. Single tap = **armed**.
3. Tilt forward/back for throttle, roll left/right to steer. Dead zone 8 degrees, full speed at 35.
4. **Sharp shake = e-stop.** Double tap, rover halts immediately, re-arm possible after 1.5 s.
5. If the rover stops answering while armed, the controller gives a long buzz every 2 s. The rover also stops
   itself after 300 ms without commands, so a dead controller never leaves it driving.

If you wear the sensor at an angle, hold your natural neutral position and press `z` in the monitor to zero it.

Controller serial keys: `a` arm, `d` disarm, `z` zero tilt, `s` status stream, `b` buzz, `c` gyro recal, `?` help.

## Tuning

Everything lives in `include/config.h`:

| Setting | What it does |
|---|---|
| `TILT_DEADZONE_DEG`, `TILT_FULL_DEG` | how much tilt before it moves, and where 100 % is |
| `PITCH_SIGN`, `ROLL_SIGN` | flip if forward or left feel reversed |
| `ARM_HOLD_MS`, `ESTOP_SHAKE_G` | arm hold time, shake strength that disarms |
| `MOTOR_MIN_PERCENT` | lowest duty the N20s actually turn at |
| `MOTOR_SLEW_PER_TICK` | acceleration limit, protects the gearboxes |
| `CMD_TIMEOUT_MS` | rover self-stop when commands vanish |

## Troubleshooting

- **No COM port**: the DevKits use a Silicon Labs CP210x bridge. Windows normally has the driver.
- **Upload times out**: hold BOOT while esptool says `Connecting...`, or lower `upload_speed` to 460800.
- **`UnicodeEncodeError` during upload**: esptool's progress bar vs. the Windows console code page. The flash still completes. `flash.ps1` sets UTF-8 first; or run `setx PYTHONUTF8 1` once.
- **Stage 4 sees no heartbeats**: both boards must be in STA mode on the same channel; this firmware never joins Wi-Fi, so both sit on channel 1. Check that the second board is a classic ESP32 (an S3/C3 needs a different `board` in `platformio.ini`).
- **Stage 5 finds nothing at 0x60**: re-check pin 1 (3V3) and pin 6 (GND). If the scan is flaky, add two 4.7 k pull-ups on SDA/SCL via a breadboard.
- **Motors twitch but don't turn**: motor power terminal not powered, or `MOTOR_MIN_PERCENT` too low.
- **WHO_AM_I not 0x68**: clone die. Same register map; nothing to fix.
