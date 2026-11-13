# 快速开始

本文以 `bk7259_robot` 为第一阶段参考板卡。SDK 和 `bk_solution_ai` 保持在框架仓库之外，由平台清单锁定版本。没有 SDK 时可以完成清单、示例入口和 host CMake 检查。

## 环境

- macOS 或 Linux
- Python 3
- CMake 3.16 或更高版本
- C 编译器
- 目标固件构建需要 Beken Armino SMP `release/v4.0.1.11`，commit `584b947920c414230f9909375f008e7d6a2c6c6b`
- `bk7259_robot` 完整产品还需要匹配的 `bk_solution_ai` `release/v4.0.1.11`，commit `a1e865e156e8dc31dfb360219c33a6a6d908ea98`

## 第一个无 SDK 检查

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
python3 tools/check_examples.py
python3 -m pytest tools/tests -q
```

当前环境没有 pytest 时，可以先运行前两个清单检查，再按 CI 安装 pytest。示例入口可以单独构建：

```bash
cmake -S examples/01_wifi_provision -B /tmp/bk-example-wifi
cmake --build /tmp/bk-example-wifi
/tmp/bk-example-wifi/01_wifi_provision
```

## 获取依赖和诊断版本

依赖默认放在 `.bk-deps/`。`fetch` 会使用平台清单中的仓库、ref 和 SHA，`doctor` 在进入 SDK 构建前检查 checkout：

```bash
./tools/bk fetch --profile bk7259_minimal --root .bk-deps
./tools/bk doctor --profile bk7259_minimal --root .bk-deps
./tools/bk fetch --profile bk7259_robot --root .bk-deps
./tools/bk doctor --profile bk7259_robot --root .bk-deps
./tools/bk env --profile bk7259_robot --root .bk-deps
```

机器人 profile 才会获取和检查 `bk_solution_ai`。最小 profile 不引入该依赖。

## 按里程碑选择示例

- M1 从 `examples/01_wifi_provision` 和 `examples/02_lan_device` 开始。它们只依赖公共网络和板卡边界，可在无 SDK 主机上完成入口检查。
- M2 使用 `examples/03_audio_player`、`04_camera_stream`、`05_peripheral_control`、`07_ui_control`、`08_ota_update`。媒体、驱动、GPIO、UI 和 OTA 能力需要 SDK 与板级适配，音频和摄像头仍要经过硬件门禁。
- M3 使用 `examples/06_robot_reference` 组合公共组件。Agora 和 AI 云端能力通过产品插件注入，插件不是公共框架依赖。

## SDK 原生构建边界

框架保留 Armino 原生 Make/Kconfig。锁定 checkout 后，按照对应 SDK 和产品工程的 AP/CP 构建说明生成 `all-app.bin`。本仓库的普通 CI 只运行 host C/CMake 检查，不把缺少 SDK 的环境伪装成固件构建通过。

硬件验收项目、PASS 证据格式以及当前待执行项见 [`docs/release-checklist.md`](../release-checklist.md)。
