# I-26 firmware source

This folder is the **portable program body** for aircraft I-26. It is intentionally kept independent from GitHub Actions and the download web page so that the entire `i26` folder can be copied, zipped, or handed to another person as-is.

## Target

- Board: Raspberry Pi Pico (RP2040)
- Pico SDK used by CI: 2.3.0
- C++: C++17
- External dependency: Eigen 3.4.0-compatible fixed revision `3147391d946bb4b6c68edd901f2add6ac1f31f8c`

The firmware is **not** configured for Pico W, Pico 2, or Pico 2 W.

## Files

```text
i26/
├─ CMakeLists.txt
├─ README.md
├─ pico_copter.cpp / .hpp
├─ control.cpp / .hpp
├─ ekf.cpp / .hpp
├─ sensor.cpp / .hpp
├─ pwm.cpp / .hpp
├─ radio.cpp / .hpp
└─ lsm9ds1_reg.c / .h
```

## Build this folder by itself

Install the ARM toolchain and Raspberry Pi Pico SDK, then run:

```bash
cmake -S . -B build \
  -DPICO_SDK_PATH=/path/to/pico-sdk \
  -DPICO_BOARD=pico \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

The UF2 is generated at:

```text
build/i26.uf2
```

If `EIGEN3_INCLUDE_DIR` is not supplied, CMake downloads the fixed Eigen revision automatically. For an offline build, provide an existing Eigen directory:

```bash
-DEIGEN3_INCLUDE_DIR=/path/to/eigen
```

## PWM outputs

The PWM GPIO assignment is centralized in `pwm.hpp`.

| Output | GPIO | Pico physical pin |
|---|---:|---:|
| RL (Rear Left) | 2 | 4 |
| RR (Rear Right) | 3 | 5 |
| FL (Front Left) | 4 | 6 |
| FR (Front Right) | 5 | 7 |
| Servo | 6 | 9 |

PWM slice and channel numbers are **not hard-coded**. `pwm.cpp` derives them from each GPIO with the Pico SDK helpers `pwm_gpio_to_slice_num()`, `pwm_gpio_to_channel()`, and `pwm_set_gpio_level()`.

To change the wiring, edit only these definitions in `pwm.hpp`:

```cpp
constexpr uint PWM_PIN_RL    = 2;
constexpr uint PWM_PIN_RR    = 3;
constexpr uint PWM_PIN_FL    = 4;
constexpr uint PWM_PIN_FR    = 5;
constexpr uint PWM_PIN_SERVO = 6;
```

The 400 Hz control-loop IRQ uses `PWM_MAINLOOP_PIN`, currently tied to `PWM_PIN_FL`, so its PWM slice also follows the GPIO assignment automatically.

When changing PWM pins, avoid GPIOs already used by other I-26 functions: UART uses GPIO 0/1, the LSM9DS1 uses GPIO 8/9/10/11/13, and the onboard LED uses GPIO 25. Also avoid choosing two GPIOs that map to the same PWM slice **and** channel; `pwm_init()` checks for this and stops at startup if such a collision is detected.

## Pico and LSM9DS1 wiring

| Raspberry Pi Pico | LSM9DS1 board | Purpose |
|---|---|---|
| GPIO 8 (pin 11), MISO/SPI0 RX | SDO (both) | Sensor data to Pico |
| GPIO 9 (pin 12), SPI0 CSn | CSM | Magnetometer chip select |
| GPIO 10 (pin 14), SCK/SPI0 SCK | SCL | SPI clock |
| GPIO 11 (pin 15), MOSI/SPI0 TX | SDA | Pico data to sensor |
| GPIO 13 (pin 17), SPI0 CSn | CSAG | Accelerometer/gyro chip select |

## Sharing

To hand the program to someone else, share this `i26` directory or the automatically generated `i26-firmware-source.zip`. The recipient does not need the repository's `.github/` or `web/` directories to build the firmware locally.

## Hardware caution

This is flight-controller firmware capable of commanding actuators. During development and first-power tests, keep propellers/rotors removed or otherwise make the drive system physically safe.
