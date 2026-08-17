# SparkFun-For-Nordic

SparkFun ICM-20948 on Nordic / Zephyr

[简体中文](README.zh-CN.md)

`SparkFun-For-Nordic` is a pure-C Zephyr port of the SparkFun ICM-20948 Arduino Library 1.3.2 for the nRF54L15 DK. The port keeps the platform-independent C core and replaces the Arduino C++/`Wire`/`Serial`/`delay()` layer with Zephyr device-tree configuration, I2C APIs, logging, and thread scheduling.

This directory is the intended content of the public `SparkFun-For-Nordic` repository. It is an engineering reference port with runnable examples, not a drop-in Zephyr Sensor API driver or a production qualification package.

## Status

- `quat6` and `quat9` were run on the target board after the directory reorganization.
- The current `quat6` source includes a later Euler-angle change that still needs a fresh build and board test.
- `raw_accel` and `multiple_sensors` were migrated in the current source tree but have not yet been rebuilt or board-tested.
- The current implementation is I2C-only, uses polling, and does not use the sensor interrupt pin.

## Tested setup and hardware warning

| Item | Current project setting |
| --- | --- |
| Board target | `nrf54l15dk/nrf54l15/cpuapp` |
| SDK | nRF Connect SDK / Toolchain 3.4.0 |
| Sensor module used during testing | GY-ICM20948V2 |
| Zephyr bus | TWIM22 |
| SCL / SDA | P1.11 / P1.12 |
| Bus speed | 100 kHz |
| I2C address | `0x69` |

The board test used a 2.3 V module input. The actual idle-high voltage on SCL/SDA at both the Nordic pins and the module pins has not been recorded. Measure it before treating this wiring as safe for nRF54L15 1.8 V GPIO. Module input voltage and I2C pull-up voltage must not be assumed to be the same.

## What is included

- A Zephyr I2C SERIF adapter using `i2c_write_dt()` and `i2c_write_read_dt()`.
- ICM-20948 and AK09916 identity checks, reset, and basic initialization.
- SparkFun's pure-C register, FIFO, DMP, and data-parsing core.
- DMP firmware loading with readback verification. The 14,301-byte image is compiled into read-only data when DMP is enabled.
- DMP examples for Quat6, Quat9, raw accelerometer data, and multiple sensors.
- Explicit error logging and FIFO recovery after malformed or failed reads.

The Nordic-specific code does not include `ICM_20948.cpp`, `ICM_20948.h`, Arduino APIs, or the Arduino `setup()`/`loop()` model.

## Repository layout

```text
.
├── CMakeLists.txt                 # Example selection and driver sources
├── prj.conf                       # Zephyr I2C and logging configuration
├── app.overlay                    # TWIM22, pins, speed, and sensor address
├── icm20948_app.h                 # Public application-side context/API
├── icm20948_port.c                # Zephyr I2C SERIF and base initialization
├── icm20948_dmp_init.c            # Pure-C DMP initialization sequence
├── examples/
│   ├── quat6/main.c               # Quat6 plus Euler-angle logging
│   ├── quat9/main.c               # Quat9 and heading accuracy logging
│   ├── raw_accel/main.c           # DMP raw accelerometer output
│   └── multiple_sensors/main.c    # Quat6, Accel, Gyro, and Compass output
└── third_party/icm20948/          # SparkFun-derived C core and DMP image
```

## Build and flash

Initialize an nRF Connect SDK 3.4.0 environment and run the following command from this directory:

```sh
west build --no-sysbuild -p always \
  -d build/quat6 \
  -b nrf54l15dk/nrf54l15/cpuapp . \
  -- -DICM20948_EXAMPLE=quat6
```

> **nRF Connect for VS Code:** Disable **Sysbuild** in the build configuration. This project is a single-image Zephyr application. Use a separate build directory such as `build/<example>` and add the matching CMake argument, for example `-DICM20948_EXAMPLE=quat9` for the `quat9` example.

Available values for `ICM20948_EXAMPLE` are:

| Example | Output | Current verification |
| --- | --- | --- |
| `quat6` | Game Rotation Vector and Euler angles in millidegrees | Base path tested; latest Euler change pending retest |
| `quat9` | Nine-axis quaternion and heading accuracy | Board-tested after reorganization |
| `raw_accel` | DMP accelerometer frames | Not yet board-tested |
| `multiple_sensors` | Quat6 at 5 Hz; raw Accel/Gyro/Compass at 1 Hz | Not yet board-tested |

Use a separate build directory for each example. Replace both `quat6` occurrences in the command when building another example:

```sh
west flash -d build/quat6 -r jlink
```

Select the flash runner appropriate for your setup if it is not J-Link.

## Public release boundary

The following split is the recommended first public release boundary.

| Path | Publish? | Reason and condition |
| --- | --- | --- |
| `CMakeLists.txt`, `prj.conf`, `app.overlay`, `.gitignore` | Yes | Required to reproduce the Zephyr application and its board wiring. |
| `icm20948_app.h`, `icm20948_port.c`, `icm20948_dmp_init.c` | Yes, after licensing decision | These are the Nordic adaptation files. Add a license for your own adaptation before publishing. |
| `examples/**` | Yes | They are the public usage and validation surface. Keep the verification status honest. |
| `third_party/icm20948/**` | Yes, with attribution | These files contain SparkFun-derived C code and DMP firmware. Keep `License.md`, preserve copyright notices, and state the 1.3.2 source version. |
| `README.md`, `README.zh-CN.md` | Yes | Public setup, limitations, provenance, and release boundary. |
| `AGENTS.md`, `HANDOFF.md`, `.codex/**` | No | Internal instructions, work history, and Codex memory. |
| `build/**`, `.cache/**`, `.DS_Store` | No | Local generated output or metadata. |
| `reference/**` | No, not as a copy | It is the full Arduino reference library. Link to the upstream repository instead of duplicating it in this port. |
| Legacy `SparkFun-Nordic/` workspace directory | No | It is a separate historical/minimal project outside this migration's public unit. |
| `docs/**` | No, not as-is | The current notes contain private workspace paths and internal handoff context. Publish only a reviewed, path-independent subset if needed. |

## Before the first public push

- Choose and add a root license for the Nordic adapter and example code. The SparkFun MIT notice in `third_party/icm20948/License.md` does not automatically license the new Nordic files.
- Keep the SparkFun attribution and license with the derived C core and DMP image. The upstream reference is [SparkFun ICM-20948 Arduino Library](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary), version 1.3.2.
- Publish this directory as the repository root, rather than pushing the whole workspace.
- Perform a clean build for all four examples; board-test the latest `quat6` Euler code, `raw_accel`, and `multiple_sensors` before describing them as verified.
- Measure SCL/SDA idle-high voltage at both ends of the cable/module while using the tested power arrangement. Fix the electrical interface if it does not meet the nRF54L15 GPIO limits.
- Review the final staged file list for local paths, internal notes, generated artifacts, and credentials before pushing.

## Known limitations

- I2C only; SPI and interrupt-driven FIFO handling are not included.
- The current device-tree overlay is specific to TWIM22, P1.11/P1.12, 100 kHz, and address `0x69`.
- The port does not expose Zephyr's generic Sensor API.
- Bias persistence, interrupt support, and a reusable multi-instance driver interface are outside the current scope.
- DMP firmware occupies about 14 KiB of read-only flash and is enabled unconditionally by the current application CMake configuration.

## License and attribution

`third_party/icm20948/License.md` contains the SparkFun code and firmware license text. The Nordic adapter and examples currently have no separate root license declaration; add one before treating the repository as fully open-source. This README makes no claim that SparkFun endorses this port.
