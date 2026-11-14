# BK7258/BK7259 开源应用框架设计（Rev 5 · 组件实现完成，目标平台门禁待验证）

> **唯一事实来源说明（Single Source of Truth）**：本设计规范与 [实施计划](../plans/2026-11-12-bk-open-framework-implementation.md)、[架构深度评审与演进规范](../../reviews/2026-11-13-architecture-review-and-espressif-comparison.md)、[乐鑫组件移植指南](../../espressif-component-porting-guide.md) 保持严格同步。当前处于 **Tier 1 Host 已通过、锁定 SHA 目标构建待复现、实机验收待执行** 阶段。

## 1. 目标与背景

建设一套面向 BK7258、BK7259 模组客户和机器人应用开发者的开源应用框架。框架建立在 Armino/BK AVDK SDK 之上，复用 SDK 已有的芯片驱动、网络协议栈、蓝牙、音频编解码、图形和系统能力，提供具备实质状态机与安全契约的应用层机制组件、板级适配、乐鑫组件兼容层（`bk_esp_compat`）、示例和文档。

第一阶段（Phase 1/2）唯一验证目标硬件为 **`bk7259_robot`（BK7259，`release/v4.0.1.11`）**，重点让客户能够快速构建可靠产品；BK7258（`release/v3.1.1.x`）作为 Phase 3 远期平台扩展，不阻塞 BK7259 交付。

## 2. 成功标准

- 新用户可以在统一仓库中完成环境配置、版本检查、选择板卡、编译并烧录第一个示例。
- 业务代码通过统一公共 API 与 Capability Contract（能力契约）访问能力，芯片差异不泄漏到业务代码。
- 每个公共组件具备实质性状态机、并发安全与生命周期契约（拒绝无价值的纯改名薄包装），不再在每个 Demo 中复制配网、重连、KV 存储、混音、帧池、按键消抖和灯效代码。
- 提供 `bk_esp_compat` 兼容层与 `idf_component_register` CMake 桥接器，为经过版本锁定和目标构建验证的 Tier A/B 组件提供迁移基础。
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
- **版本字段无歧义约束**：manifest 统一使用 `tag_object_sha` 与 `peeled_commit_sha`，不使用含糊的 `sha` 字段。
- 组件必须细粒度拆分、能够通过 Kconfig 独立启用或关闭，关闭时零符号进入最终镜像。
- 公共框架采用 Apache-2.0；Agora、商用云 SDK 等专有依赖作为可选插件；参考第三方开源协议（如 MIT 许可的小智 WebSocket 协议）需独立模块并保留完整许可证声明。

## 5. 架构

```text
应用示例 / 产品模板（products/bk7259_robot，仅调用公共 C API，无芯片宏）
        |
产品策略组件（bk_motion / bk_robot_face / bk_battery_policy）与可选插件（plugin_agora）
        |
框架机制组件（事件、安全存储、配网、音频会话/混音、视频帧池、按键、灯效、AI协议、乐鑫兼容层 bk_esp_compat + 暂缓目标能力 bk_ota）
        |
公共抽象接口 + Capability Contract（能力契约、所有权、线程与错误码规范）
        |
平台后端与板级适配层（bk7259_v4 / posix_sim / 远期 bk7258_v3 + bk_board）
        |
Armino/BK AVDK SDK（外置锁定）
```

### 5.1 SDK 复用边界

直接依赖 Armino SDK 的能力包括：RTOS 任务和同步原语、Wi-Fi/BLE 协议栈、TCP/IP、基础外设驱动、音频编解码、LVGL 显示栈、Flash/文件系统、`bk_frame_buffer` / `cache` 原语和底层 BL2/OTA 能力。

框架新增的能力包括：异步事件总线、带敏感凭证加密钩子的 KV 存储、板级能力契约与外设句柄初始化、配网与退避重连状态机、音频会话背压与提示音混音、引用计数视频帧池与 Cache 同步封装、GPIO/ADC 按键与 PWM 灯效状态机、独立的小智 WebSocket/Opus/MCP/语音控制帧协议适配，以及乐鑫 ESP-IDF/ESP-IoT-Solution 组件兼容层（`bk_esp_compat`）。

### 5.2 细粒度组件分层（默认机制层 vs 暂缓目标组件 vs 产品策略层）

1. **默认交付的基础与核心机制组件（`components/`）**：
   - `bk_common`：统一错误码（`bk_err.h`）、版本宏、日志与 Capability Contract 基础定义。
   - `bk_port`：OS 原语、CSPRNG、SRAM/PSRAM 内存分配统计（封装 `os_sram_malloc` / `psram_malloc` / `psram_cache_malloc`）、D-Cache 同步原语（封装 `bk_dcache_clean_for_producer` / `bk_dcache_invalidate_for_consumer`）。
   - `bk_core_event`：独立工作线程的异步事件总线与组件生命周期管理。
   - `bk_storage`：KV 持久化存储、CRC32 校验、敏感凭证加密钩子（`BK_STORAGE_FLAG_SENSITIVE`）与安全擦除（`bk_storage_erase_ns`）。
   - `bk_health`（可选）：SRAM/PSRAM 水位采样、高水位/泄漏阈值告警（`OK`/`WARNING`/`CRITICAL`）与任务健康诊断。
   - `bk_board`：板卡静态验证能力表（区分 `verified` 与 `unverified`）与已核实外设句柄初始化。
   - `bk_net`：Wi-Fi 状态机、BLE/SoftAP 配网编排、多 AP 凭证管理、指数退避重连、SNTP 与 TLS 证书校验检查。
   - `bk_button`（可选）：GPIO 与 ADC 分压阶梯按键（含 +25mV 施密特回差）消抖、单击/双击/多击/长按触发与长按保持周期事件状态机。
   - `bk_led_effect`（可选）：LED 优先级抢占队列、带定时自动恢复的临时抢占（`bk_led_set_effect_timed`）与二次伽马呼吸 PWM 波形生成器（`bk_led_tick_ms`）。
   - `bk_audio_session`（可选）：单路 20ms PCM 采集（含 AEC 开关）/播放会话与下行 Ringbuffer 背压管理。
   - `bk_audio_mixer`（可选）：本地提示音与实时语音流的 Ducking / Preempt 混音器。
   - `bk_video_frame_pool`（可选）：基于 64 字节对齐 PSRAM 的原子引用计数视频帧池与 Cache 同步接口封装。
   - `bk_ui_core`（可选）：LVGL 递归线程互斥锁（`lock/unlock`）与显示刷新生命周期。
   - `bk_ai_agent`（可选）：拆分为底层 `transport_websocket`（RFC 6455 + mbedTLS CA/SNI）与独立的 `protocol_xiaozhi`（Hello 握手、Listen `start/stop/detect`、Abort、TTS/LLM Emotion/STT 解析、原生 `libopus` 3×20ms<->60ms 编解码、MCP JSON-RPC 2.0 主路径）。
   - `bk_esp_compat`（可选）：ESP-IDF / ESP-IoT-Solution 兼容层（提供 `idf_component_register` CMake 桥接、`esp_err`、`esp_log`、`esp_check`、`esp_heap_caps`、`esp_timer`、`esp_event`、`nvs_flash`、`iot_button`、`led_indicator`）。
2. **暂缓目标能力组件（`deferred_target_components`）**：
   - `bk_ota`：已完成状态机、264KB 分区拦截、媒体管线挂起（`BK_ERR_OTA_MEDIA_BUSY`）、防回滚计数器、64KB 断电续传检查点、CRC32 和签名验证器缺失时的拒绝路径。真实 EC-P256 验证、公钥配置和候选分区表仍待实机证据，在实机完成 5 项前置证据测量前保持列于 `deferred_target_components`。
3. **产品策略组件（`product_components/` 及 `products/bk7259_robot/`，不进入 `minimal` 依赖）**：
   - `bk_motion`：舵机关键帧三次 Hermite S-Curve 加减速插补、机械限位保护（Yaw `[-90,+90]`、Pitch `[-35,+35]`）、多段关键帧序列与预设动作（点头、摇头、环视、归中）。
   - `bk_robot_face`：机器人表情映射（对接小智 LLM emotion 字符串）、周期眨眼状态机与 TTS 播报口型同步动画。
   - `bk_battery_policy`：电池 4 点滑动平均抗瞬态跌落滤波、+50mV 施密特回差迟滞、锂电池分段 OCV SoC 估算。
   - `products/bk7259_robot`：集成上述全部机制与策略组件的机器人产品级主控器（`bk_robot_product`）。
4. **可选商业插件（`plugins/`）**：
   - `plugin_agora`：声网 RTC/RTM 适配插件隔离边界。

## 6. 四级验证门禁（4-Tier Verification Gate）

1. **Tier 1（Host 行为门禁 - `posix_sim`）**：使用 PyYAML 6.0.3 和 libopus 动态库完成 `15/15` CTest 与 `12/12` Python 单测。验证记录必须包含依赖来源。
2. **Tier 2（SDK 编译与裁剪门禁 - `bk7259_v4`）**：锁定 SHA 的 AP/CP ELF 与 `all-app.bin` 仍待复现并保存产物摘要。
3. **Tier 3（故障注入与压力门禁）**：验证弱网断线重连、SNTP 失败/过期证书拒绝、满缓冲背压。
4. **Tier 4（真实板卡验收门禁 - `bk7259_robot`）**：按 [docs/release-checklist.md](../../release-checklist.md) 逐项在实机硬件上完成验收并填入真实日志与数据。

## 7. 分阶段增量交付状态

1. **Step 0（构建探针与静态证据）**：已完成 `probe.sh`、静态 SDK 事实核查和许可证审计。锁定 SHA 容器构建证据仍待补齐。
2. **Step 1（M1 最小公共层与联网）**：已交付 `./tools/bk`、`bk_common`、`bk_port`、`bk_core_event`、`bk_storage`、`bk_board`、`bk_net` 及 `01_wifi_provision`、`02_lan_device`。
3. **Step 2（M2 单一媒体与基础外设）**：已交付 `bk_button`（GPIO+ADC）、`bk_led_effect`（定时抢占+PWM）、`bk_audio_session`、`bk_audio_mixer`、`bk_video_frame_pool`、`bk_ui_core` 及示例 `03`~`05`。
4. **Step 3（M3 小智协议、机器人产品工程与乐鑫兼容层）**：已交付 `bk_ai_agent`、`product_components/*`、`products/bk7259_robot`、`bk_esp_compat`、`plugins/plugin_agora` 及 `06_robot_reference`。
5. **Step 4（`bk_ota` 状态机与候选分区表交付，待实机尺寸证据解封默认交付）**：已交付 `components/bk_ota`、`test_ota_fsm` 与 `auto_partitions_ota.csv`。
