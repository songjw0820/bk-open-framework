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

## SDK 原生构建与 Docker 一键编译（`dbuild.sh`）

框架对齐 Beken 官方 SDK 的 `dbuild.sh` 容器化构建方式（基于官方镜像 `bekencorp/armino-idk`，最低版本 `1.5`），无需在宿主机手动安装交叉编译器与 Python 依赖库即可编译 `all-app.bin`：

1. **拉取或加载官方构建镜像**：
   ```bash
   docker pull bekencorp/armino-idk:1.5
   # 或从博通下载站加载离线镜像：
   # docker load -i bekencorp-armino-idk-v1.5.tar
   ```

2. **拉取锁定版本的 SDK 与 Solution（若尚未拉取）**：
   ```bash
   ./tools/bk fetch --profile bk7259_robot --root .bk-deps
   ./tools/bk doctor --profile bk7259_robot --root .bk-deps
   ```

3. **使用 `dbuild.sh` 编译固件**：
   ```bash
   ./dbuild.sh make bk7259
   ```
   `dbuild.sh` 会自动将 `.bk-deps/bk_avdk_smp` 挂载为 `/armino`、`.bk-deps/bk_solution_ai` 挂载为 `/solution`、当前框架仓库挂载为 `/bk-open-framework`，将 `bk_open_framework` 组件注入 AP/CP 双核构建，并在构建结束后自动清理临时挂载目录。

   也可以通过发布门禁脚本开启 Docker 模式进行完整校验：
   ```bash
   BK_USE_DOCKER=1 BK_DEPS_ROOT=$PWD/.bk-deps ./tools/sdk_release_gate.sh
   ```
   生成的烧录固件位于 `.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/package/all-app.bin`。

硬件验收项目、PASS 证据格式以及当前待执行项见 [`docs/release-checklist.md`](../release-checklist.md)。
