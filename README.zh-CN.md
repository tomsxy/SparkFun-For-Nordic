# SparkFun-For-Nordic

SparkFun ICM-20948 Nordic / Zephyr 驱动移植

[English](README.md)

`SparkFun-For-Nordic` 是 SparkFun ICM-20948 Arduino Library 1.3.2 面向 nRF54L15 DK 的纯 C Zephyr 移植。项目保留平台无关的 C 核心，并用 Zephyr 设备树、I2C API、日志和线程调度替换 Arduino 的 C++、`Wire`、`Serial` 和 `delay()` 层。

本目录是建议的公开 `SparkFun-For-Nordic` 仓库内容。它是一个可运行示例驱动移植和工程参考，不是可直接接入 Zephyr Sensor API 的通用驱动，也不是经过生产级认证的完整软件包。

## 当前状态

- 目录重组后的 `quat6` 和 `quat9` 已在目标板上运行验证。
- 当前 `quat6` 源码后来增加了 Euler 角输出，尚未针对这次最新源码重新构建和板测。
- 当前源码树中的 `raw_accel` 和 `multiple_sensors` 已完成移植，但尚未重新构建或板测。
- 当前实现仅支持 I2C，采用轮询方式，未使用传感器中断引脚。

## 已测试配置与硬件警告

| 项目 | 当前工程配置 |
| --- | --- |
| 目标板 | `nrf54l15dk/nrf54l15/cpuapp` |
| SDK | nRF Connect SDK / Toolchain 3.4.0 |
| 测试使用的传感器模块 | GY-ICM20948V2 |
| Zephyr 总线 | TWIM22 |
| SCL / SDA | P1.11 / P1.12 |
| 总线速率 | 100 kHz |
| I2C 地址 | `0x69` |

板上测试时模块输入供电为 2.3 V。模块输入电压和 SCL/SDA 空闲高电平（Nordic 引脚侧、模块引脚侧）尚未完成实测记录。在确认电平符合 nRF54L15 1.8 V GPIO 规格前，不要把当前接法视为安全方案。模块输入电压与 I2C 上拉电压不能默认视为同一件事。

## 包含内容

- 使用 `i2c_write_dt()` 和 `i2c_write_read_dt()` 的 Zephyr I2C SERIF 适配层。
- ICM-20948 和 AK09916 的身份检查、复位与基础初始化。
- SparkFun 纯 C 寄存器、FIFO、DMP 和数据解析核心。
- 带回读校验的 DMP 固件加载。启用 DMP 时，14,301 字节固件以只读数据编译进程序。
- Quat6、Quat9、原始加速度和多传感器 DMP 示例。
- 明确的错误日志，以及读取异常后的 FIFO 恢复。

Nordic 代码不包含 `ICM_20948.cpp`、`ICM_20948.h`、Arduino API 或 Arduino 的 `setup()`/`loop()` 生命周期模型。

## 目录结构

```text
.
├── CMakeLists.txt                 # 示例选择和驱动源文件
├── prj.conf                       # Zephyr I2C 与日志配置
├── app.overlay                    # TWIM22、引脚、速率和传感器地址
├── icm20948_app.h                 # 应用侧上下文和 API
├── icm20948_port.c                # Zephyr I2C SERIF 与基础初始化
├── icm20948_dmp_init.c            # 纯 C DMP 初始化流程
├── examples/
│   ├── quat6/main.c               # Quat6 与 Euler 角日志
│   ├── quat9/main.c               # Quat9 与 heading accuracy 日志
│   ├── raw_accel/main.c           # DMP 原始加速度输出
│   └── multiple_sensors/main.c    # Quat6、Accel、Gyro、Compass 输出
└── third_party/icm20948/          # SparkFun 派生 C 核心与 DMP 固件
```

## 构建与烧录

初始化 nRF Connect SDK 3.4.0 环境后，在本目录执行：

```sh
west build --no-sysbuild -p always \
  -d build/quat6 \
  -b nrf54l15dk/nrf54l15/cpuapp . \
  -- -DICM20948_EXAMPLE=quat6
```

> **Nordic VS Code 插件（nRF Connect for VS Code）：** 在 Build Configuration 中关闭 **Sysbuild**。本项目是单镜像 Zephyr 应用。每个示例使用独立的构建目录，例如 `build/<example>`，并在 CMake arguments 中输入对应参数；例如构建 `quat9` 时使用 `-DICM20948_EXAMPLE=quat9`。

`ICM20948_EXAMPLE` 可选值如下：

| 示例 | 输出 | 当前验证状态 |
| --- | --- | --- |
| `quat6` | Game Rotation Vector 与毫度单位 Euler 角 | 基础流程已测试；最新 Euler 改动待复测 |
| `quat9` | 九轴四元数与 heading accuracy | 目录重组后已板测 |
| `raw_accel` | DMP 加速度帧 | 尚未板测 |
| `multiple_sensors` | Quat6 为 5 Hz；原始 Accel/Gyro/Compass 为 1 Hz | 尚未板测 |

每个示例使用独立的构建目录。构建其他示例时，同时替换命令中的两个 `quat6`：

```sh
west flash -d build/quat6 -r jlink
```

如果使用的不是 J-Link，请替换为对应的烧录 runner。

## 建议的公开边界

以下划分适合作为第一次公开发布的边界。

| 路径 | 是否公开 | 原因与条件 |
| --- | --- | --- |
| `CMakeLists.txt`、`prj.conf`、`app.overlay`、`.gitignore` | 是 | 复现 Zephyr 应用和板级接线所需。 |
| `icm20948_app.h`、`icm20948_port.c`、`icm20948_dmp_init.c` | 是，适用 `LICENSE-NORDIC.md` | 这是 Nordic 适配代码；MIT 作用域只覆盖带 SPDX 声明的 XyShen 原创贡献。 |
| `examples/**` | 是，需保留作用域署名 | 这是公开使用方式和验证入口；必须如实保留验证状态，SparkFun 派生内容继续适用原许可证。 |
| `third_party/icm20948/**` | 是，但必须保留署名和许可 | 其中包含派生自 SparkFun 的 C 代码和 DMP 固件；保留 `License.md`、版权声明，并说明来源版本为 1.3.2。 |
| `LICENSE-NORDIC.md` | 是，有明确作用域 | MIT 许可证只覆盖 XyShen 的 Nordic 适配和示例原创贡献，不是整个仓库的通用许可证。 |
| `README.md`、`README.zh-CN.md` | 是 | 公开配置、限制、来源和发布边界。 |
| `AGENTS.md`、`HANDOFF.md`、`.codex/**` | 否 | 内部指令、工作历史和 Codex 记忆。 |
| `build/**`、`.cache/**`、`.DS_Store` | 否 | 本地生成物或元数据。 |
| `reference/**` | 不直接复制 | 这是完整 Arduino 参考库；在移植仓库中链接上游即可，不必重复存放。 |
| 工作区中的旧 `SparkFun-Nordic/` 目录 | 否 | 它是本迁移之外的独立历史/最小工程。 |
| `docs/**` | 不应原样公开 | 当前整理文档含工作区私有路径和内部交接上下文；如有需要，只发布审阅后的、与本地路径无关的内容。 |

## 后续公开更新前的检查清单

- 保持 `LICENSE-NORDIC.md` 只覆盖 XyShen 的 Nordic 适配和示例原创贡献；它不会替代 SparkFun 许可证，也不覆盖 `third_party/icm20948/**`。
- 对派生 C 核心和 DMP 固件保留 SparkFun 署名及许可证，并注明上游为 [SparkFun ICM-20948 Arduino Library](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary) 1.3.2。
- 将本目录作为仓库根目录公开，不要把整个工作区一起推送。
- 对四个示例分别执行干净构建；在 README 把最新 `quat6` Euler、`raw_accel` 和 `multiple_sensors` 写成“已验证”之前，先完成板测。
- 在当前供电接法下，实测线缆/模块两端 SCL/SDA 空闲高电平；若不符合 nRF54L15 GPIO 限制，先修正电气接口。
- 推送前检查最终暂存文件列表，确认没有本地路径、内部笔记、构建产物和凭据。

## 已知限制

- 仅支持 I2C；不包含 SPI 和基于中断的 FIFO 处理。
- 当前设备树 overlay 固定使用 TWIM22、P1.11/P1.12、100 kHz 和地址 `0x69`。
- 没有提供 Zephyr 通用 Sensor API。
- 偏置持久化、中断支持和可复用的多实例驱动接口不在当前范围内。
- DMP 固件占用约 14 KiB 只读 Flash；当前应用的 CMake 配置会无条件启用它。

## 许可证与署名

`LICENSE-NORDIC.md` 将 MIT 许可证适用于带 SPDX 声明的 XyShen Nordic 适配和示例原创贡献。`third_party/icm20948/License.md` 仍是 SparkFun 代码和固件的许可证；本仓库不重新授权这些内容。本文不表示 SparkFun 认可或背书本移植。
