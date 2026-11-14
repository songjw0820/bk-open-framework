# BK Open Framework

面向 BK7259、BK7258 模组应用开发的开源框架，建立在 Beken Armino SMP 之上。

当前第一阶段以 `bk7259_robot` 为产品目标，同时提供不依赖产品 solution 的 `bk7259_minimal` 集成路径。上游 SDK 和 solution 保持外置，通过 Profile 清单锁定版本和依赖边界。

## 项目状态

| 验证项 | 状态 |
| --- | --- |
| Host CTest | 13/13 通过 |
| Python 工具测试 | 12/12 通过 |
| Docker ARM AP/CP 目标构建 | 已验证，生成 AP/CP ELF 和 `all-app.bin` |
| 锁定 SDK SHA 的目标构建 | 待执行 |
| BK7259 实机验收 | 待执行 |

目标构建验证使用 `bekencorp/armino-idk:1.5` 容器和当前 SDK checkout。Profile 中锁定的 SDK 提交仍需单独完成复现构建，不能用容器构建结果替代实机验收。

## 已实现能力

- Wi-Fi 配网、局域网服务、网络状态机、凭据保存和自动重连
- 音频会话、混音和媒体缓冲区
- RFC 6455 WebSocket/WSS、证书链和 SNI 配置
- 原生 libopus 编解码路径
- Xiaozhi 协议和 MCP JSON-RPC 处理
- BK7259 AP/CP 适配和打包路径
- 机器人产品组件及示例

## 平台 Profile

| Profile | 用途 | 上游依赖 |
| --- | --- | --- |
| [`bk7259_minimal`](platforms/bk7259_minimal.yaml) | 只验证框架与 SDK 的最小集成 | Armino SMP |
| [`bk7259_robot`](platforms/bk7259_robot.yaml) | 机器人产品集成 | Armino SMP、`bk_solution_ai` |

当前 Profile 锁定：

- Armino SMP `release/v4.0.1.11`
- `bk_solution_ai` `release/v4.0.1.11`（仅 `bk7259_robot` 使用）

## Host 快速开始

```bash
git clone https://github.com/songjw0820/bk-open-framework.git
cd bk-open-framework

python3 -m venv .venv
. .venv/bin/activate
python -m pip install pyyaml

cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

检查可用 Profile 和外部 SDK：

```bash
./tools/bk list-profiles
./tools/bk doctor --profile bk7259_minimal --sdk-dir /path/to/bk_avdk_smp
```

## Docker 目标构建

仓库提供组件级目标构建探针，适合在带有 Armino 工具链的容器中验证 Profile、AP/CP ELF 和 `all-app.bin`：

```bash
docker run --rm \
  -v "$PWD":/work/framework:ro \
  -v "$HOME/sdk/bk_avdk_smp":/work/sdk \
  -w /work/framework \
  bekencorp/armino-idk:1.5 \
  bash -lc '
    ./tools/spikes/bk7259_component_probe/probe.sh \
      --profile minimal \
      --sdk-dir /work/sdk \
      --build \
      --clean-build
  '
```

验证机器人 Profile 时，还需要挂载可访问的 `bk_solution_ai` checkout：

```bash
docker run --rm \
  -v "$PWD":/work/framework:ro \
  -v "$HOME/sdk/bk_avdk_smp":/work/sdk \
  -v "$HOME/sdk/bk_solution_ai":/work/solution:ro \
  -w /work/framework \
  bekencorp/armino-idk:1.5 \
  bash -lc '
    ./tools/spikes/bk7259_component_probe/probe.sh \
      --profile robot \
      --sdk-dir /work/sdk \
      --solution-dir /work/solution \
      --build \
      --clean-build
  '
```

探针会把框架组件复制到隔离的临时工程目录，再调用 SDK 的构建入口；不会修改 SDK 或 solution 的源文件。`--clean-build` 会清理挂载 SDK 下对应的生成目录，适合独立的构建 checkout 使用。完整参数见 [`probe.sh`](tools/spikes/bk7259_component_probe/probe.sh)。

## 目录结构

```text
platforms/   平台 Profile 和上游版本清单
tools/       版本检查、构建和验证工具
components/  可复用框架组件
docs/        架构、发布检查和依赖审计文档
```

## 当前限制

- Profile 锁定的 Armino SMP 提交为 `584b947920c414230f9909375f008e7d6a2c6c6b`，仍需完成该 SHA 的 Docker 目标构建复现。
- 尚未完成 BK7259 实机刷写、启动、WSS、Opus、CSPRNG、音频链路和长稳测试。
- `bk_ota` 仍按发布检查清单延期，待 OTA 状态契约和端到端验证完成后接入。

详细证据和剩余门禁见 [`docs/release-checklist.md`](docs/release-checklist.md)、[`docs/platforms/bk7259_v4.0.1.11.md`](docs/platforms/bk7259_v4.0.1.11.md) 和 [`docs/legal/bk_solution_ai-dependency-inventory.md`](docs/legal/bk_solution_ai-dependency-inventory.md)。

## License

Apache-2.0
