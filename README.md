# BK Open Framework

面向 BK7259 模组应用开发的开源框架，建立在 Beken Armino SMP 之上。

第一阶段只支持 BK7259 的 `bk7259_robot` 板卡和 `minimal`/`robot` 两种 profile，锁定：

- Armino SMP (`bekencorp/bk_avdk_smp`) `release/v4.0.1.11`（commit `584b947920c414230f9909375f008e7d6a2c6c6b`）
- `bekencorp/bk_solution_ai` `release/v4.0.1.11`（commit `a1e865e156e8dc31dfb360219c33a6a6d908ea98`）

仓库结构包括：

- `platforms/`：芯片、板卡和上游 SDK 版本锁定清单（`bk7259_minimal.yaml`、`bk7259_robot.yaml`）
- `boards/`：板级能力与引脚定义（`boards/bk7259_robot`）
- `components/`：公共框架组件（`bk_core`、`bk_board`、`bk_net`、`bk_ota`、`bk_peripheral`、`bk_media`、`bk_ui`）
- `products/`：产品级组装与可选插件边界（`products/bk7259_robot`）
- `examples/`：分阶段示例工程（`01_wifi_provision` 至 `08_ota_update`）
- `tools/`：依赖获取、版本诊断与构建门禁工具
- `dbuild.sh`：对齐博通官方 `bekencorp/armino-idk` 的 Docker 一键固件编译脚本
- `docs/`：快速开始、架构设计、迁移指南、平台证据与发布验收清单

---

## 编译方式

### 1. 拉取并校验锁定版本的官方 SDK 与 Solution

上游 SDK 与 Solution 保持外置（默认位于 `.bk-deps/`），通过 [tools/bk](tools/bk) 拉取并校验版本：

```bash
# 拉取 bk7259_robot 所需的 bk_avdk_smp 与 bk_solution_ai
./tools/bk fetch --profile bk7259_robot

# 校验本地 checkout 的分支/Tag 与 40 位 commit SHA 是否完全匹配
./tools/bk doctor --profile bk7259_robot
```

### 2. 固件编译方式一（推荐）：Docker 容器编译（`dbuild.sh`）

参照博通官方 SDK 的 `dbuild.sh` 方式，使用官方 `bekencorp/armino-idk`（版本 >= `1.5`）容器编译，无需在宿主机手动安装交叉编译工具链和 Python 依赖：

```bash
# 1) 拉取博通官方构建镜像（或通过 docker load -i 加载离线镜像包）
docker pull bekencorp/armino-idk:1.5

# 2) 一键执行 AP + CP 双核固件编译与打包
./dbuild.sh make bk7259
```

也可以通过发布门禁脚本开启 Docker 模式进行完整校验（含 `bk doctor`、双核静态库检查与 `all-app.bin` SHA256 计算）：

```bash
BK_USE_DOCKER=1 BK_DEPS_ROOT=$PWD/.bk-deps ./tools/sdk_release_gate.sh
```

### 3. 固件编译方式二：宿主机本地交叉编译

若宿主机已安装官方交叉编译器 `arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi`（默认查找 `/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin` 或通过 `BK_TOOLCHAIN_PATH` 指定）：

```bash
# 运行完整 SDK 发布门禁（执行 bk doctor + AP/CP 编译 + 校验 all-app.bin）
BK_DEPS_ROOT=$PWD/.bk-deps ./tools/sdk_release_gate.sh

# 或单独调用组件注入与构建脚本
./tools/spikes/bk7259_component_probe/probe.sh \
  --solution-dir .bk-deps/bk_solution_ai \
  --sdk-dir .bk-deps/bk_avdk_smp \
  --build
```

#### 编译产物路径
- **完整烧录固件**：`.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/package/all-app.bin`
- **OTA 升级固件**：`.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/package/app_pack.rbl`
- **框架 AP 静态库**：`.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/bk7259_ap/armino/bk_open_framework/libbk_open_framework.a`
- **框架 CP 静态库**：`.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/bk7259/armino/bk_open_framework/libbk_open_framework.a`

### 4. Host 端无 SDK 校验与单元测试（CMake / CTest）

在未安装 SDK 工具链的主机或普通 CI 环境中，可直接运行清单校验、组件单元测试与示例构建：

```bash
# 清单、示例元数据与工具单元测试
python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
python3 tools/check_examples.py
python3 -m pytest tools/tests -q

# 构建并运行产品级 Host 冒烟测试（无 Agora 插件 / 注入 Agora 插件）
cmake -S products/bk7259_robot -B build/robot/no-plugin \
  -DBK7259_ROBOT_BUILD_TESTS=ON -DBK7259_ROBOT_ENABLE_AGORA=OFF
cmake --build build/robot/no-plugin --parallel
ctest --test-dir build/robot/no-plugin --output-on-failure

# 单独构建并运行示例（以 01_wifi_provision 为例）
cmake -S examples/01_wifi_provision -B build/examples/01_wifi_provision
cmake --build build/examples/01_wifi_provision
./build/examples/01_wifi_provision/01_wifi_provision
```

更多说明请参考 [`docs/getting-started/README_CN.md`](docs/getting-started/README_CN.md)、[`docs/platforms/bk7259_v4.0.1.11.md`](docs/platforms/bk7259_v4.0.1.11.md) 与 [`docs/release-checklist.md`](docs/release-checklist.md)。
