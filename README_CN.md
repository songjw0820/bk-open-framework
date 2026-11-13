# BK Open Framework

BK Open Framework 面向 BK7259 模组应用开发，构建于 Beken Armino SMP 之上。

第一阶段只支持 BK7259 的 `bk7259_robot` 板卡和 `minimal`、`robot` 两种 profile，平台清单位于 `platforms/`：

- `bk7259_minimal.yaml`：只锁定 Armino SMP SDK。
- `bk7259_robot.yaml`：额外锁定 `bk_solution_ai`。

每个清单都固定上游仓库、release ref 和 40 位 Git SHA。可使用以下命令检查清单：

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
```

公共组件放在 `components/`，板卡适配放在 `boards/`，示例和产品集成分别放在
`examples/` 与 `products/`，工具位于 `tools/`。SDK 与 solution 保持外置，其他芯片、板卡和 profile 当前不在支持范围内，完整工具链和硬件验证结果将由后续任务补充。

## SDK 获取与版本诊断

工具命令从 `platforms/<profile>.yaml` 读取唯一版本来源。依赖默认放在仓库下的 `.bk-deps/`，也可以通过 `--root` 指定目录。

```bash
./tools/bk doctor --profile bk7259_robot
./tools/bk doctor --profile bk7259_robot --root /path/to/deps
./tools/bk doctor --profile bk7259_robot --dry-run
./tools/bk fetch --profile bk7259_minimal --root /path/to/deps
./tools/bk fetch --profile bk7259_robot --root /path/to/deps
./tools/bk env --profile bk7259_robot --root /path/to/deps
```

`doctor` 会检查 checkout 是否存在、当前分支/Tag ref 和 HEAD SHA 是否与清单一致。机器人 profile 还会检查 `bk_solution_ai`，minimal profile 不会检查 solution。`fetch` 只会更新干净的现有 checkout，发现未提交改动时会直接报错以避免覆盖。`--dry-run` 只打印锁定版本，不执行构建或网络操作。

## 固件编译

### 方式一：Docker 容器编译（推荐，`./dbuild.sh`）

对齐博通官方 `bekencorp/armino-idk`（版本 >= `1.5`）容器构建方式：

```bash
# 拉取官方构建镜像
docker pull bekencorp/armino-idk:1.5

# 一键编译 BK7259 AP/CP 双核固件并打包 all-app.bin
./dbuild.sh make bk7259

# 或通过发布门禁脚本以 Docker 模式执行完整校验
BK_USE_DOCKER=1 BK_DEPS_ROOT=$PWD/.bk-deps ./tools/sdk_release_gate.sh
```

### 方式二：宿主机本地工具链编译

宿主机已安装 `arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi` 时，可直接运行：

```bash
BK_DEPS_ROOT=$PWD/.bk-deps ./tools/sdk_release_gate.sh
```

编译生成的烧录固件位于 `.bk-deps/bk_solution_ai/projects/beken_robot/build/bk7259/beken_robot/package/all-app.bin`。完整指引见 [`docs/getting-started/README_CN.md`](docs/getting-started/README_CN.md)。
