# BK7258/BK7259 开源应用框架设计（Rev 3 · 已对齐架构评审与契约规范）

> **唯一事实来源说明（Single Source of Truth）**：本设计规范与 [实施计划](../plans/2026-11-12-bk-open-framework-implementation.md)、[架构深度评审与演进规范](../../reviews/2026-11-13-architecture-review-and-espressif-comparison.md) 保持严格同步。

## 1. 目标与背景

建设一套面向 BK7258、BK7259 模组客户和机器人应用开发者的开源应用框架。框架建立在 Armino/BK AVDK SDK 之上，复用 SDK 已有的芯片驱动、网络协议栈、蓝牙、音频编解码、图形和系统能力，提供具备实质状态机与安全契约的应用层机制组件、板级适配、示例和文档。

第一阶段（Phase 1/2）唯一验证目标硬件为 **`bk7259_robot`（BK7259，`release/v4.0.1.11`）**，重点让客户能够快速构建可靠产品；BK7258（`release/v3.1.1.x`）作为 Phase 3 远期平台扩展，不阻塞 BK7259 交付。

## 2. 成功标准

- 新用户可以在统一仓库中完成环境配置、版本检查、选择板卡、编译并烧录第一个示例。
- 业务代码通过统一公共 API 与 Capability Contract（能力契约）访问能力，芯片差异不泄漏到业务代码。
- 每个公共组件具备实质性状态机、并发安全与生命周期契约（拒绝无价值的纯改名薄包装），不再在每个 Demo 中复制配网、重连、KV 存储、混音、帧池、按键消抖和灯效代码。
- 至少提供三类可验证示例：最小联网设备、音视频/外设设备、开源 AI 机器人应用。
- 文档能够清晰说明“直接使用 SDK 的能力”、“本框架新增的机制组件”与“产品策略组件/可选插件”之间的边界。

## 3. 非目标

- 不重新实现 Armino SDK 已有的 Wi-Fi、BLE、TCP/IP、RTOS、基础驱动、音频编解码和图形底层。
- 第一阶段不建设完整的在线组件市场或远程依赖服务。
- 第一阶段不承诺未经验证的“全自动 CP 工程配置生成器（Auto-CP）”，而是采用来源隔离的受控 CP 模板。
- 第一阶段不将 BK7258 实机支持纳入发布门槛（归入 Phase 3）。

## 4. 约束

- 以当前 Armino SMP/BK AVDK 构建系统和 Kconfig 机制为基础。
- **阶段边界约束**：Phase 1/2 只实现 `bk7259_v4` 与 `posix_sim`（主机行为单测后端）；`bk7258_v3` 仅预留接口契约。
- **Minimal 与 Solution 严格解耦约束**：
  - `bk7259_minimal` profile **仅依赖 `bk_avdk_smp` SDK**，其受控 CP 模板直接取自 `bk_avdk_smp/projects/app/cp`，严禁直接或间接引用 `bk_solution_ai` 的任何文件。
  - 只有 `bk7259_robot` profile 才锁定并引用 `bk_solution_ai`（`projects/beken_robot/cp` 及机器人参考资源）。
- 组件必须细粒度拆分、能够通过 Kconfig 独立启用或关闭，关闭时零符号进入最终镜像。
- 公共框架采用 Apache-2.0；Agora、商用云 SDK 等专有依赖作为可选插件；参考第三方开源协议（如 MIT 许可的小智 WebSocket 协议）需独立模块并保留完整许可证声明。

## 5. 架构

```text
应用示例 / 产品模板（仅调用公共 C API，无芯片宏）
        |
产品策略组件（bk_motion / bk_robot_face / bk_battery_policy）与可选插件（plugin_agora）
        |
框架机制组件（事件、安全存储、配网、音频会话/混音、视频帧池、按键、灯效、OTA、AI协议）
        |
公共抽象接口 + Capability Contract（能力契约、所有权、线程与错误码规范）
        |
平台后端与板级适配层（bk7259_v4 / posix_sim / 远期 bk7258_v3 + bk_board）
        |
Armino/BK AVDK SDK（外置锁定）
```

### 5.1 SDK 复用边界

直接依赖 Armino SDK 的能力包括：RTOS 任务和同步原语、Wi-Fi/BLE 协议栈、TCP/IP、基础外设驱动、音频编解码、LVGL 显示栈、Flash/文件系统、`bk_frame_buffer` / `cache` 原语和底层 BL2/OTA 能力。

框架新增的能力包括：异步事件总线、带敏感凭证加密钩子的 KV 存储、板级能力契约与外设句柄初始化、配网与退避重连状态机、音频会话背压与提示音混音、引用计数视频帧池与 Cache 同步封装、按键/灯效状态机、OTA 前置媒体挂起与安全校验编排、独立的小智 WebSocket/Opus/MCP 协议适配。

### 5.2 细粒度组件分层（机制层 vs 产品策略层）

1. **基础与核心机制组件（`components/`）**：
   - `bk_common`：统一错误码（`bk_err.h`）、版本宏、日志与 Capability Contract 基础定义。
   - `bk_port`：OS 原语、SRAM/PSRAM 内存分配统计（封装 `os_sram_malloc` / `psram_malloc` / `psram_cache_malloc`）、D-Cache 同步原语（封装 `bk_dcache_clean_for_producer` / `bk_dcache_invalidate_for_consumer`）。
   - `bk_core_event`：独立工作线程的异步事件总线与组件生命周期管理。
   - `bk_storage`：KV 持久化存储、CRC32 校验、敏感凭证加密钩子（`BK_STORAGE_FLAG_SENSITIVE`）与安全擦除（`bk_storage_erase_ns`）。
   - `bk_health`（可选）：SRAM/PSRAM 水位采样与任务健康诊断。
   - `bk_board`：板卡静态验证能力表（区分 `verified` 与 `unverified`）与已核实外设句柄初始化。
   - `bk_net`：Wi-Fi 状态机、BLE/SoftAP 配网编排、多 AP 凭证管理、指数退避重连、SNTP 与 TLS 证书校验检查。
   - `bk_button`（可选）：GPIO/ADC 按键消抖与单击/双击/长按状态机。
   - `bk_led_effect`（可选）：LED 优先级闪烁/呼吸状态队列。
   - `bk_audio_session`（可选）：单路 20ms PCM 采集（含 AEC 开关）/播放会话与下行 Ringbuffer 背压管理。
   - `bk_audio_mixer`（可选）：本地提示音与实时语音流的 Ducking / Preempt 混音器。
   - `bk_video_frame_pool`（可选）：基于 64 字节对齐 PSRAM 的原子引用计数视频帧池与 Cache 同步封装。
   - `bk_ui_core`（可选）：LVGL 线程互斥锁（`lock/unlock`）与显示刷新生命周期。
   - `bk_ota`（可选）：升级前媒体管线挂起协同、镜像校验与下载状态管理（完整安全签名、防回滚与分区重算作为目标能力受平台证据门禁约束）。
   - `bk_ai_agent`（可选）：拆分为底层 `transport_websocket` 与独立的 `protocol_xiaozhi`（JSON + 60ms Opus 聚合 + MCP JSON-RPC 2.0 主路径；传统 `iot` 标记为 deprecated 仅做可选兼容）。
2. **产品策略组件（`product_components/`，不进入 `minimal` 依赖）**：
   - `bk_motion`：舵机/马达关键帧平滑插补序列器。
   - `bk_robot_face`：机器人表情动画视图。
   - `bk_battery_policy`：电池充放电策略。
3. **可选商业插件（`plugins/`）**：
   - `plugin_agora`：声网 RTC/RTM 适配插件。

## 6. 仓库结构

```text
bk-open-framework/
  platforms/
    bk7259_minimal.yaml
    bk7259_robot.yaml
  components/
    bk_common/
    bk_port/
    bk_core_event/
    bk_storage/
    bk_health/
    bk_board/
    bk_net/
    bk_button/
    bk_led_effect/
    bk_audio_session/
    bk_audio_mixer/
    bk_video_frame_pool/
    bk_ui_core/
    bk_ota/
    bk_ai_agent/
      transport_websocket/
      protocol_xiaozhi/
  product_components/
    bk_motion/
    bk_robot_face/
    bk_battery_policy/
  plugins/
    plugin_agora/
  boards/
    bk7259_robot/
  examples/
    01_wifi_provision/
    02_lan_device/
    03_audio_player/
    04_camera_stream/
    05_peripheral_control/
    06_robot_reference/
  products/
    bk7259_robot/
  docs/
    platforms/
    legal/
    reviews/
    superpowers/
    release-checklist.md
  tools/
    bk
    spikes/
```

## 7. 四级验证门禁（4-Tier Verification Gate）

1. **Tier 1（Host 行为门禁 - `posix_sim`）**：仅验证纯状态机、事件顺序、CRC/序列化、退避计时、引用计数、20ms->60ms 帧聚合、混音算术与协议报文解析（不能替代 IPC、DMA/Cache、射频与硬件验证）。
2. **Tier 2（SDK 编译与裁剪门禁 - `bk7259_v4`）**：验证 AP/CP 外部组件注入、Kconfig 关闭时零残留符号、`bk7259_minimal`（仅依赖 `bk_avdk_smp`）与 `bk7259_robot` 完整生成 `all-app.bin`。
3. **Tier 3（故障注入与压力门禁）**：验证弱网断线重连、SNTP 失败/过期证书拒绝、满缓冲背压、OTA 媒体挂起失败中止与断电恢复。
4. **Tier 4（真实板卡验收门禁 - `bk7259_robot`）**：按 [docs/release-checklist.md](../../release-checklist.md) 逐项在实机硬件上完成验收。

## 8. 版本与依赖

| 芯片 | 阶段 | Armino SMP 维护分支 | AI solution 维护分支 | 生产依赖方式 |
|---|---|---|---|---|
| BK7259 | Phase 1 / Phase 2（当前） | `release/v4.0.1` (`release/v4.0.1.11`) | `release/v4.0.1` (`release/v4.0.1.11`) | `minimal` 仅锁定 SDK；`robot` 同时锁定配对的 SDK 与 solution 标签及 SHA |
| BK7258 | Phase 3（远期扩展） | `release/v3.1.1` | `release/v3.1.1` | 待 BK7259 `v0.1.0` 发布后再行引入 |

## 9. 分阶段增量交付范围

1. **Step 0（前置阻塞证据闭环）**：在具备锁定版 `bk_avdk_smp` 与工具链的环境中跑通 Task 0 AP/CP 空组件编译与 `all-app.bin` 打包；核实 `bk7259_robot` 分区表（解决 `ota` 分区仅 264KB 的限制）与引脚定义。
2. **Step 1（M1 最小公共层与联网）**：交付 `./tools/bk`、`bk_common`、`bk_port`、`bk_core_event`、`bk_storage`、`bk_board`、`bk_net` 及 `01_wifi_provision`、`02_lan_device`。
3. **Step 2（M2-A 单一媒体与基础外设）**：增量交付 `bk_button`、`bk_led_effect`、`bk_audio_session`、`bk_video_frame_pool`、`bk_ui_core` 及示例 `03`~`05`。
4. **Step 3（M2-B 安全 OTA）**：在重规划验证后的分区表基础上交付 `bk_ota` 并完成实机升级/异常拒绝验证。
5. **Step 4（M3 小智协议与机器人参考工程）**：交付 `bk_ai_agent`（`transport_websocket` + `protocol_xiaozhi` MCP 主路径）、`product_components/*`、`plugins/plugin_agora` 及 `06_robot_reference`，通过 [docs/release-checklist.md](../../release-checklist.md) 发布 `v0.1.0`。
