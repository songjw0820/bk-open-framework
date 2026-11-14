# BK Open Framework 架构深度评审与演进规范（Rev 2 · 已闭合 Codex 评审 P0/P1/P2 项）

- **文档路径**：`docs/reviews/2026-11-13-architecture-review-and-espressif-comparison.md`
- **版本状态**：`Rev 2.0`（根据 Codex 针对 `b2a1c7c` 的评审报告全面修订，收敛第一阶段范围、补齐安全/生命周期/实时性契约与四级验证门禁）
- **关联现有文档**：
  - 现有设计规范：[docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](../superpowers/specs/2026-11-12-bk-open-framework-design.md)
  - 现有实施计划：[docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md)
  - 平台探针证据：[docs/platforms/bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)
  - 依赖审计清单：[docs/legal/bk_solution_ai-dependency-inventory.md](../legal/bk_solution_ai-dependency-inventory.md)

---

## 零、 Codex 评审问题闭环对照表（Changelog from Rev 1）

| 等级 | 评审问题编号与摘要 | Rev 2 修订措施与落位章节 |
| :--- | :--- | :--- |
| **P0-1** | **实施范围与原始决策冲突**：过早引入 BK7258 `v3.1.1` 及相关板卡/示例，违背“第一阶段只验证 `bk7259_robot`”的约束。 | **已修正（见 1.3 & 六）**：Phase 1/2 严格锁定 **BK7259 (`release/v4.0.1.11`) + `bk7259_robot`** 单平台，仅实现 `bk7259_v4` 与 `posix_sim` 后端；`BK7258 (v3.1.1)` 仅保留抽象接口契约，整体后移至 **Phase 3**，绝不阻塞 BK7259 交付。 |
| **P0-2** | **Auto-CP 是未经验证的架构假设**：现有证据仅确认了 `usr_gpio_cfg.h` 路径与构建入口，未证明 CP 工程可由 YAML 安全生成。 | **已修正（见 3.2）**：废除 Phase 1 全自动生成 CP 的假设，改为**两阶段受控演进**：Phase 1 采用**受控 CP 模板（Controlled CP Template）+ 显式字段注入校验 + diff/编译门禁**；仅当 Task 0 实机闭环后再评估自动生成。 |
| **P0-3** | **`bk_ai_agent` 协议兼容声明过度 & 许可证事实错误**：混淆小智（MIT）与 OpenAI Realtime 协议差异，试图用单一接口掩盖握手/音频/事件差异。 | **已修正（见 4.6）**：明确标注 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 为 **MIT 许可证**；将架构拆分为 `transport_websocket`（传输层）+ `protocol_xiaozhi` / `protocol_openai_realtime`（独立状态机与 wire-format）+ `plugin_agora`（商业插件），每个协议单独验收。 |
| **P0-4** | **OTA 与凭证安全缺少发布级契约**：缺少凭证加密/擦除、签名校验、防回滚、断电恢复及媒体挂起失败处理；“SNTP 完成则 TLS 必过”表述不成立。 | **已修正（见 4.2, 4.3, 4.5）**：新增凭证加密与恢复出厂安全擦除契约；修正 SNTP 与 TLS 证书链/SNI/吊销校验边界；补齐 OTA 签名、Anti-rollback、双分区回滚、块级校验断电续传及媒体管线挂起失败拒绝升级契约。 |
| **P1-5** | **“100% 芯片无关”不成立**：音视频、网络、UI 强依赖底层 DMA、PSRAM、Cache、AEC/NPU 与 IPC 硬件属性。 | **已修正（见 3.1 & 四）**：改为 **`公共 API + Capability Contract（能力查询契约）+ 平台后端（bk7259_v4 / posix_sim / 未来 bk7258_v3）`**，业务层不写芯片宏，但通过能力契约显式处理硬件限制与错误码。 |
| **P1-6** | **组件职责过宽**：`bk_core`、`bk_media`、`bk_peripheral` 聚合过多功能，不利于 Kconfig 裁剪与最小镜像体积控制。 | **已修正（见 3.3）**：拆分为细粒度**机制层组件**（`bk_core_event`, `bk_storage`, `bk_health`, `bk_audio_session`, `bk_audio_mixer`, `bk_video_frame_pool`, `bk_button`, `bk_led_effect`）与**产品策略组件**（`bk_motion`, `bk_robot_face`, `bk_battery_policy`）。 |
| **P1-7** | **C API 缺少关键生命周期与并发契约**：缓存所有权、回调线程上下文、跨线程安全性、幂等性与 `deinit` 契约缺失。 | **已修正（见 四 各组件契约表）**：为所有核心 API 补充完整的所有权（Ownership）、线程上下文（Thread Context）、重入性、阻塞语义及配对 `stop/destroy/deinit` 生命周期契约。 |
| **P1-8** | **音视频方案缺少实时约束**：采样率、重采样位置、DMA/Cache 一致性、背压丢帧、AEC 参考时钟及 BSP 能力边界未定义。 | **已修正（见 4.4）**：明确音频格式矩阵、重采样位置、DMA 32字节对齐与 D-Cache Clean/Invalidate 责任、满缓冲背压策略（阻塞 vs 丢帧错误码）以及 `bk_board` 未支持能力时的明确报错语义。 |
| **P1-9** | **POSIX “100% 跑通”表述不现实**：POSIX 无法验证 IPC、DMA/Cache、AEC 声学、PSRAM 带宽与 `all-app.bin` 链接。 | **已修正（见 五）**：建立 **Host 行为单测 -> SDK 编译链接 -> 硬件模拟/故障注入 -> 真实板卡验收** 四级验证门禁（4-Tier Verification Gate），明确界定 POSIX 的验证边界。 |
| **P2** | **文档事实、链接与乐鑫对标校准**：文件计数基准、未实测硬件能力措辞、ESP-IDF/BSP/ADF 能力边界校准。 | **已修正（见 一 & 二）**：标明基线提交 `4c031f7` 的 10 个文件事实；严格区分“已确认静态配置”与“待实机验证硬件能力”；准确界定 ESP-IDF、ESP-BSP、ESP-ADF 的真实工程边界。 |

---

## 一、 现状诊断与阶段范围收敛（Current State & Scope Alignment）

### 1.1 仓库与工程现状事实核查（基于基线提交 `4c031f7`）
1. **工程进度处于 Task 0（静态探针与规划阶段）**：
   - 基线提交 `4c031f7` 共包含 10 个文件（本评审文档加入后为 11 个文件），仅含设计规范、实施计划、依赖清单、2 个平台 YAML（[bk7259_minimal.yaml](../platforms/bk7259_minimal.yaml)、[bk7259_robot.yaml](../platforms/bk7259_robot.yaml)）及入口检查脚本（[probe.sh](../tools/spikes/bk7259_component_probe/probe.sh)）。
   - `components/`、`boards/`、`examples/`、`products/` 尚未创建，尚无 C 代码实现。
   - **构建与硬件验证现状**：由于当前环境尚无锁定的 `bk_avdk_smp` 源码树与交叉工具链，AP/CP 空组件编译与 `all-app.bin` 打包状态均为 `not_run_toolchain_unavailable`（见 [bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)）。
   - **板级硬件证据边界**：目前 `bk7259_robot` 上**唯一完成静态代码确认的引脚配置**是 CP 侧 UART0 使用 `GPIO10 (RX)` / `GPIO11 (TX)`（源自 `cp/config/bk7259/usr_gpio_cfg.h`）；双麦克风、扬声器、MIPI CSI 摄像头、LCD、按键、LED、振动马达、ToF、环境光、G-Sensor、NFC 等均属于**文档或源码声明存在、但尚未经本仓库编译与实机验证的能力**。

### 1.2 现有设计的合理决策（必须保持）
- **上游外置与版本配对锁定**：通过 manifest 锁定 `bk_avdk_smp`（`release/v4.0.1.11`, SHA `24cce68adccebec25a7c3bb22e0d1de00f0da5cb`）与 `bk_solution_ai`（`release/v4.0.1.11`, SHA `009c95182de07ae874e39e3d3c24994431bcec70`）。
- **最小 Profile 解耦**：`bk7259_minimal` 只依赖 `bk_avdk_smp` SDK，不强依赖 `bk_solution_ai`。
- **专有依赖隔离**：声网（Agora）等闭源商业库不进入核心仓库，核心框架保持 `Apache-2.0` 许可证。

### 1.3 阶段范围收敛（解决 P0-1：消除范围冲突）
为严格遵守已确认的第一阶段工程约束，同时确保框架未来可扩展至 BK7258 模组生态，明确划分三个阶段边界：
- **Phase 1（单平台基础闭环）**：**唯一目标硬件为 `bk7259_robot`（BK7259，`release/v4.0.1.11`）**。完成 Task 0 编译与打包证据闭环，落地最小公共机制组件（`bk_core_event`、`bk_storage`、`bk_board`、`bk_net`、`bk_button`、`bk_led_effect`）与最小联网/局域网示例。后端仅实现 `bk7259_v4` 与 `posix_sim`（主机行为模拟）。
- **Phase 2（BK7259 媒体、安全 OTA 与开源 AI 协议落地）**：仍在 `bk7259_robot` 单平台上，按增量顺序逐个落地 `bk_audio_session`、`bk_video_frame_pool`、`bk_ota`（含完整安全契约）及 `bk_ai_agent`（独立小智协议适配器与可选 Agora 插件）。
- **Phase 3（BK7258 `release/v3.1.1.x` 跨芯片扩展与模组泛化）**：在 BK7259 全部实机验收通过并发布 `v0.1.0` 后，再新增 `platforms/bk7258_*.yaml` 与 `bk7258_v3` 后端适配。**BK7258 的任何工作不得进入 Phase 1/2 的交付门槛，也不阻塞 BK7259 发布。**

---

## 二、 对标乐鑫（Espressif）开源生态的客观校准

为什么现有规划中的组件给人“没有实质功能”的观感？因为现有实施计划将组件定位成了底层 SDK 的“薄包装（thin adapters）”。对标乐鑫生态时，我们需要吸取其组件化设计精髓，同时客观认清乐鑫各子项目的真实工程边界：

| 对标维度 | 乐鑫生态的真实工程机制与边界 | 当前 `bk-open-framework` 规划的不足 | 校准后的重构设计原则 |
| :--- | :--- | :--- | :--- |
| **1. 构建与组件模型**<br>([ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)) | 核心优势在于**每个组件具备独立的 `CMakeLists.txt` + `Kconfig` + `idf_component.yml` 显式依赖声明**，未启用的组件不参与编译链接，工具链与构建入口高度统一。 | 尚未完成外部组件注入 AP/CP 构建的实测（Task 0 未闭环），组件间依赖与 Kconfig 裁剪粒度过粗。 | 先通过 Task 0 验证 Armino `EXTRA_COMPONENT_DIRS`（或同等机制）在 AP/CP 两侧的注入与裁剪；每个组件保持细粒度拆分，关闭 Kconfig 时零代码体积开销。 |
| **2. 板级支持包模式**<br>([ESP-BSP How-To](https://github.com/espressif/esp-bsp/blob/master/docu/how_to_use.md), [BSP API 示例](https://github.com/espressif/esp-bsp/blob/master/bsp/m5stack_tab5/API.md)) | `ESP-BSP` 为**具体开发板**提供句柄级初始化接口（如 `bsp_display_start()`、`bsp_audio_codec_speaker_init()`、`bsp_display_lock()`），但它是**按板卡逐个编写的板级封装**，并非跨所有硬件的万能自动生成器。 | `bk_board` 仅规划了 `has_capability()` 和 `get_pin()`，业务层拿到引脚号后仍需自己写大量板级胶水代码；而 Rev 1 提出的“全自动生成 CP 配置”又跨过了安全验证边界。 | **受控板级封装 + Capability 契约**：`bk_board` 针对已验证板卡（Phase 1 仅 `bk7259_robot`）提供显示/音频/摄像头的板级初始化封装；对未验证或不存在的硬件能力严格返回 `BK_ERR_NOT_SUPPORTED`。 |
| **3. 音频与多媒体管线**<br>([ESP-ADF 官方仓库](https://github.com/espressif/esp-adf)) | `ESP-ADF` 提供了音频管线、混音与编解码抽象，但在实际工程中也面临着**与不同 ESP-IDF 版本分支的兼容性矩阵压力**以及重度管线带来的内存开销。 | `bk_media` 在原计划中只有空泛的薄包装，既无语音会话生命周期管理，也无视频帧引用计数池；而一次性实现完整管线又容易重蹈过度复杂的覆辙。 | **机制与策略分层、增量交付**：不造臃肿的通用图管线引擎；拆分为独立的 `bk_audio_session`（单路语音采集/播放）、`bk_audio_mixer`（可选提示音混音）和 `bk_video_frame_pool`（引用计数帧池），先跑通单一能力再组合。 |
| **4. 外设与系统组件**<br>(`ESP-IoT-Solution`) | `iot_button`、`led_indicator` 等组件之所以高频复用，是因为它们只做一件事并把状态机做透（消抖、多击、灯效优先级），且不绑定特定产品业务。 | `bk_peripheral` 把按键、LED、舵机、传感器、电池全部塞进同一个组件，导致最小联网设备也会被迫引入无关依赖。 | **细粒度独立组件**：将 `bk_button`、`bk_led_effect` 作为独立基础组件；将机器人专用的舵机动作（`bk_motion`）、表情（`bk_robot_face`）剥离到产品级组件。 |
| **5. 开源 AI 语音/视觉协议**<br>([xiaozhi-esp32 仓库](https://github.com/78/xiaozhi-esp32), [WebSocket 协议文档](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)) | `xiaozhi-esp32`（**MIT 许可证**）定义了清晰的 WebSocket 握手、JSON 状态/控制消息、二进制 Opus 音频流与 MCP 工具调用规范，使开发者无需购买商业 RTC license 即可跑通端到端语音交互。 | 原计划剥离 Agora 闭源库后只保证“开机+基础联网”，导致开源用户无法体验完整的 AI 机器人功能。 | 在 Phase 2 提供独立的 **`protocol_xiaozhi` 协议适配组件**（与 `protocol_openai_realtime` 和 `plugin_agora` 严格平级解耦），让开源用户拥有开箱即用的免费 AI 交互链路。 |

---

## 三、 架构设计修正：能力契约、受控双核与细粒度组件拆分

### 3.1 跨芯片抽象边界：Capability Contract 模型（解决 P1-5）

摒弃不切实际的“L3 组件 100% 芯片无关”口号。在嵌入式系统中，DMA 对齐、PSRAM Cache 一致性、硬件 AEC、摄像头像素格式、AP/CP 核间通道均受底层芯片与 SDK 制约。正确的分层模型如下：

```text
+------------------------------------------------------------------------+
| 业务与产品层 (Products / Examples)                                     |
| - 仅调用框架公共 C API，严禁出现 #ifdef CONFIG_SOC_BK7259 等芯片宏     |
+------------------------------------------------------------------------+
                                   |
+------------------------------------------------------------------------+
| 框架公共 API 与能力契约层 (Public API + Capability Contract)           |
| - 统一的错误码体系 (BK_OK / BK_ERR_NOT_SUPPORTED / BK_ERR_NO_MEM ...)  |
| - 运行期/编译期能力查询 (如是否支持硬件 AEC、DMA 对齐字节数、最大帧率) |
| - 明确的线程上下文、缓存所有权与反初始化生命周期契约                   |
+------------------------------------------------------------------------+
          |                        |                         |
+-------------------+   +----------------------+   +---------------------+
| bk7259_v4 Backend |   | posix_sim Backend    |   | bk7258_v3 Backend   |
| (Phase 1 & 2 实现)|   | (Phase 1 & 2 实现)   |   | (Phase 3 远期扩展)  |
| - 绑定 v4.0.1.11  |   | - 仅验证纯状态机/算法|   | - 仅预留接口，不进入|
| - AP/CP IPC 适配  |   | - 不模拟 DMA/IPC/硬件|   |   Phase 1/2 交付范围|
+-------------------+   +----------------------+   +---------------------+
```

### 3.2 AP/CP 双核集成策略：从“受控模板”到“受控生成”（解决 P0-2）

针对 BK7259 的 AP + CP 双子系统架构，为防止未经验证的代码生成引入隐蔽的底层故障（如低功耗状态机异常、IPC 共享内存越界、串口与协议栈冲突），实行严格的两阶段策略：

#### 第一阶段（Phase 1 强制执行）：受控 CP 模板模式（Controlled CP Template）
1. **以官方验证工程为受控基线**：直接复用 `bk_solution_ai` (`release/v4.0.1.11`) 中经验证的 CP 工程配置作为受控只读基线模板，**不自动生成 CP 的 CMakeLists、链接脚本、分区表或低功耗策略代码**。
2. **白名单字段校验而非盲目生成**：
   - `boards/bk7259_robot/board.yaml` 仅记录**已在源码或硬件原理图中核实**的硬件事实（例如已确认的 CP UART0 `GPIO10/11`）。
   - 未经核实的能力（如尚未核对完整 pinmux 的外设）在 `board.yaml` 中必须标记为 `status: unverified`，组件尝试初始化 `unverified` 外设时在编译期或初始化期立即返回 `BK_ERR_NOT_VERIFIED`。
   - 构建前由 `./tools/bk doctor` 对比 `board.yaml` 与受控 CP 模板中的 `usr_gpio_cfg.h`，确保二者一致（Schema 校验 + Diff 校验）。
3. **外部组件注入规则**：
   - 明确区分组件的 `execution_domain`：Phase 1 的业务组件（`bk_core_event`, `bk_storage`, `bk_net`, `bk_button`, `bk_led_effect`）优先作为 **AP 侧组件**接入 `ap/CMakeLists.txt`，直接复用 SDK 已有的跨核 Wi-Fi/BLE/驱动代理能力，不在 CP 侧编写新的自定义业务代码。

#### 第二阶段（Task 0 与实机全部验证通过后）：受控局部生成
- 只有当 AP/CP 空组件注入、`all-app.bin` 完整链接以及 `bk7259_robot` 实机启动全部通过，且逐项摸清 CP 侧引脚复用表（Pinmux）、PM 睡眠唤醒源和 IPC 通道约束后，才允许工具链根据 `board.yaml` 自动生成受控的 `usr_gpio_cfg.h` 片段，且每次生成必须通过编译门禁与回归测试。

### 3.3 组件职责拆分：机制层 vs 产品策略层（解决 P1-6）

为保证 Kconfig 可独立裁剪、避免最小联网示例（`bk7259_minimal`）引入不必要的 ROM/RAM 开销，将原有宽泛组件拆分为**基础机制组件（Framework Mechanism Components）**与**产品策略组件（Product Policy Components）**：

```text
bk-open-framework/
├── components/                         # 【机制层组件】通用、细粒度、可独立开关
│   ├── bk_common/                      # 统一错误码 (bk_err.h)、版本宏、日志标签与能力契约结构体
│   ├── bk_port/                        # OS原语、SRAM/PSRAM分配统计、Cache对齐宏 (bk7259_v4 / posix_sim)
│   ├── bk_core_event/                  # 仅负责：异步事件总线与组件生命周期状态机 (无Flash/KV依赖)
│   ├── bk_storage/                     # 仅负责：KV持久化存储、CRC校验、敏感凭证加密与安全擦除
│   ├── bk_health/                      # [可选] 仅负责：SRAM/PSRAM水位采样、任务栈监控与Crash摘要记录
│   ├── bk_board/                       # 板级能力描述表与已验证外设句柄初始化 (Phase 1: bk7259_robot)
│   ├── bk_net/                         # Wi-Fi连接状态机、BLE/SoftAP配网编排、退避重连、SNTP与TLS就绪检查
│   ├── bk_ota/                         # [可选] 签名校验、防回滚、媒体挂起协同、断点续传与双分区回滚確認
│   ├── bk_button/                      # [可选] GPIO/ADC按键消抖与单击/双击/长按状态机
│   ├── bk_led_effect/                  # [可选] LED 闪烁/呼吸优先级状态队列
│   ├── bk_audio_session/               # [可选] 单路音频采集(含硬件AEC开关)/播放会话与环形缓冲背压管理
│   ├── bk_audio_mixer/                 # [可选] 本地提示音与语音流的混音/闪避(Ducking/Preempt)算子
│   ├── bk_video_frame_pool/            # [可选] PSRAM 引用计数视频帧池与 DMA/Cache 同步封装
│   ├── bk_ui_core/                     # [可选] LVGL 上下文互斥锁 (lock/unlock) 与显示刷新生命周期
│   └── bk_ai_agent/                    # [可选] AI 会话管理与协议插件框架
│       ├── transport_websocket/        # 纯 WebSocket (WSS) 帧收发与保活重连
│       ├── protocol_xiaozhi/           # 小智协议状态机 (JSON + Binary Opus + MCP)
│       └── protocol_openai_realtime/   # [远期可选] OpenAI Realtime 事件协议状态机
├── product_components/                 # 【产品策略层组件】仅在机器人或特定产品 profile 中启用
│   ├── bk_motion/                      # 舵机/电机余弦插补与关键帧动作序列器
│   ├── bk_robot_face/                  # 机器人双目/单目表情动画视图
│   └── bk_battery_policy/              # 电池充放电曲线、低电告警与自动休眠策略
└── plugins/                            # 【专有/第三方插件】
    └── plugin_agora/                   # 声网 RTC/RTM 适配插件
```

---

## 四、 核心组件实质性功能、安全契约与 C API 规范（解决 P0-3, P0-4, P1-7, P1-8）

以下规定每个核心组件必须实现的**实质功能**、**生命周期/线程/内存所有权契约**以及**安全与实时约束**。

### 4.1 `bk_core_event`（异步事件总线）

- **实质功能**：提供解耦的发布/订阅机制，内置独立事件派发线程（Event Loop Task）与有界环形队列，支持按 `domain + event_id` 精确订阅、通配符订阅、带超时的事件投递。
- **API 契约与生命周期**：
  - **幂等性**：`bk_event_loop_init()` 重复调用返回 `BK_ERR_INVALID_STATE`。
  - **内存所有权**：`bk_event_post(domain, id, data, data_len, timeout_ms)` 会将 `[data, data_len]` **深拷贝**入内部事件池（要求 `data_len <= CONFIG_BK_EVENT_MAX_PAYLOAD_SIZE`，默认 256 字节）；调用方在函数返回后可立即释放栈上或堆上的 `data`。
  - **线程上下文**：所有订阅者的 `bk_event_handler_t` 均在 **Event Loop 专属线程**中串行执行；回调内严禁执行超过 `50ms` 的阻塞操作，但允许在回调内调用 `bk_event_post()`。
  - **反初始化契约**：调用 `bk_event_loop_deinit()` 时，先拒绝新事件投递（返回 `BK_ERR_INVALID_STATE`），排空或丢弃队列剩余事件，等待当前正在执行的回调完成后销毁工作线程并释放全部订阅节点。

```c
// components/bk_core_event/include/bk_core_event.h
bk_err_t bk_event_loop_init(const bk_event_loop_cfg_t *cfg);
bk_err_t bk_event_loop_deinit(void);

bk_err_t bk_event_subscribe(bk_event_domain_t domain, int32_t event_id,
                            bk_event_handler_t handler, void *user_ctx,
                            bk_event_sub_handle_t *out_sub);
bk_err_t bk_event_unsubscribe(bk_event_sub_handle_t sub);

bk_err_t bk_event_post(bk_event_domain_t domain, int32_t event_id,
                       const void *payload, size_t payload_len, uint32_t timeout_ms);
```

### 4.2 `bk_storage`（持久化 KV 与凭证安全契约 —— 解决 P0-4）

- **实质功能**：提供带命名空间隔离、CRC32 完整性校验、RAM 读缓存的键值存储，并专门针对敏感数据（Wi-Fi 密码、云端 Token）提供加密与安全擦除支持。
- **凭证安全与断电契约（P0 强制门禁）**：
  1. **敏感凭证加密（Credential Encryption）**：凡标记为 `BK_STORAGE_FLAG_SENSITIVE` 的写入项（如 Wi-Fi PSK、AI 鉴权 Token），在 `bk7259_v4` 后端必须调用硬件加密引擎（基于芯片唯一 UID/EFUSE HUK 派生的 AES-GCM/CTR 密钥）加密后再落盘，禁止明文写入 Flash；若平台未启用硬件密钥，必须返回 `BK_ERR_STORAGE_NO_KEY`（除非显式开启开发期 Kconfig `CONFIG_BK_STORAGE_ALLOW_PLAINTEXT_DEV_ONLY`）。
  2. **恢复出厂与安全擦除（Secure Erase & Factory Reset）**：`bk_storage_erase_ns(ns)` 与 `bk_storage_factory_reset()` 必须先用全 `0x00` 覆写 RAM 缓存及对应 Flash 逻辑记录，再执行 Flash 扇区擦除，确保擦除后无法从残留缓存或未擦除页中恢复 Wi-Fi 密码。
  3. **断电原子性（Power-loss Resilience）**：采用双扇区日志结构（Append-only Log + GC 压缩），单条记录带头部 Magic、序列号与尾部 CRC32；写入中途断电重启后，初始化扫描自动丢弃未闭合的半写记录并回退至上一有效版本，绝不损坏整个分区。
- **API 线程与阻塞契约**：
  - 所有 API 内部通过互斥锁保证**多线程安全**。
  - `bk_storage_set_blob()` 先更新 RAM 缓存并标记脏位（若带 `BK_STORAGE_FLAG_SYNC` 则立即阻塞写 Flash）；`bk_storage_commit()` 为**同步阻塞调用**，直到脏页全部刷入 Flash 并完成校验后才返回。

```c
// components/bk_storage/include/bk_storage.h
#define BK_STORAGE_FLAG_DEFAULT    (0)
#define BK_STORAGE_FLAG_SENSITIVE  (1 << 0) // 强制加密存储，擦除时零化内存
#define BK_STORAGE_FLAG_SYNC       (1 << 1) // 立即同步刷入 Flash

bk_err_t bk_storage_init(const bk_storage_cfg_t *cfg);
bk_err_t bk_storage_deinit(void); // 自动执行一次 commit 后释放锁与缓存

bk_err_t bk_storage_set_blob(const char *ns, const char *key, const void *data, size_t len, uint32_t flags);
bk_err_t bk_storage_get_blob(const char *ns, const char *key, void *out_buf, size_t *inout_len);
bk_err_t bk_storage_del_key(const char *ns, const char *key);
bk_err_t bk_storage_erase_ns(const char *ns);       // 安全擦除指定命名空间（如清除全部配网凭证）
bk_err_t bk_storage_factory_reset(void);            // 擦除所有用户命名空间并重置存储区
bk_err_t bk_storage_commit(uint32_t timeout_ms);    // 同步阻塞落盘
```

### 4.3 `bk_net`（配网编排、多 AP 重连与 TLS/时间安全契约 —— 解决 P0-4）

- **实质功能**：
  1. **配网状态机与错误细分**：封装 BLE/SoftAP 配网流程，精确区分并广播配网失败原因（`BK_NET_ERR_WIFI_NOT_FOUND`、`BK_NET_ERR_AUTH_FAILED`、`BK_NET_ERR_DHCP_TIMEOUT`）。
  2. **多凭证管理与指数退避重连**：通过 `bk_storage`（启用 `BK_STORAGE_FLAG_SENSITIVE`）加密存储最多 `N`（默认 5）组 AP 凭证；断网后按指数退避（1s, 2s, 4s, 8s, 16s, 封顶 30s + 随机抖动）重连。
- **时间同步与 TLS 安全契约（修正 Rev 1 错误表述，P0 强制门禁）**：
  - **严格区分 IP 就绪、时间同步与 TLS 安全校验**：
    - 获取 IP 后进入 `BK_NET_STATE_IP_READY`。
    - 系统时间同步（通过 SNTP 或 HTTPS Date 头部校时）仅用于满足 X.509 证书有效期（`notBefore` / `notAfter`）校验的**必要非充分条件**。
    - **时间同步失败处理**：若 SNTP 在设定超时内失败，状态保持在 `BK_NET_STATE_IP_READY` 并发布 `BK_EVENT_NET_TIME_SYNC_FAILED`；此时上层发起 TLS 连接若因系统时间为 1970 年导致证书过期校验失败，必须返回明确的 `BK_ERR_TLS_CERT_TIME_INVALID`，**严禁在生产配置下因 SNTP 失败而自动关闭 TLS 证书验证**。
    - **TLS 完整校验要求**：任何云端连接（OTA、WebSocket AI）除校对时间外，必须强制启用 CA 根证书链校验、服务器域名（SNI / SAN）匹配校验及最低 TLS 1.2 协议版本约束。

### 4.4 `bk_audio_session`、`bk_audio_mixer` 与 `bk_video_frame_pool`（实时约束与 API 契约 —— 解决 P1-7, P1-8）

将媒体能力拆分为三个独立可选组件，并明确规定实时流参数、DMA/Cache 一致性与背压契约：

#### (1) `bk_audio_session`（单路语音采集与播放会话）
- **实时音频格式约束**：
  - 采集链路（Uplink）标准内部 PCM 格式固定为：`16000 Hz`（或 `8000 Hz`）、`16-bit Signed LE`、`Mono（单声道）`，每帧周期固定为 `20ms`（即 16kHz 下每帧 320 采样点 / 640 字节）。
  - AEC 参考信号（Reference）约束：在 `bk7259_v4` 上，硬件/DSP AEC 的参考信号直接绑定 codec DAC 回采通道，要求采集与播放使用同一时钟域；若板卡不支持硬件回采，则在 `bk_board` 能力契约中声明 `BK_AUDIO_CAP_HW_AEC = 0`，回退至软件 AEC 或报错 `BK_ERR_NOT_SUPPORTED`。
  - 重采样位置：若下行音源（如本地 MP3 提示音 24kHz/44.1kHz）与 DAC 硬件采样率（16kHz）不一致，重采样必须在进入 `bk_audio_mixer` 或 DMA 发送环之前完成。
- **内存所有权与背压（Backpressure）契约**：
  - `bk_audio_session_push_downlink(session, pcm_or_encoded_buf, len, timeout_ms)`：函数内部将数据**同步拷贝**至下行 Ringbuffer，返回后调用方立即拥有 `buf` 的修改/释放权。
  - **背压策略**：当消费者（DAC DMA）消费慢导致下行 Ringbuffer 满时，等待 `timeout_ms`；若超时仍满，**不静默丢数据**，而是立即返回 `BK_ERR_AUDIO_RINGBUF_FULL` 并累加 overrun 计数，由调用方决定等待或丢帧。
  - 上行回调 `bk_audio_uplink_cb_t` 在音频采集工作线程中触发（非 ISR），传入的缓冲区仅在回调函数执行期间有效，严禁在回调内阻塞超过 `5ms`。

#### (2) `bk_video_frame_pool`（PSRAM 零拷贝帧池与 DMA/Cache 契约）
- **DMA 与 Cache 一致性契约（P1-8 关键硬件约束）**：
  - 帧池中的每个数据缓冲区（`frame->data`）在 `bk7259_v4` 上从 **PSRAM** 分配，且首地址与长度强制按 **32 字节（Cache Line Size）向上对齐**。
  - **Cache 维护责任**：
    - 当硬件 DMA（如摄像头 CSI/DVP 或 JPEG 硬件编码器）直接向 `frame->data` 写入完成后，底层驱动在交付 `bk_video_frame_t*` 给上层前，必须调用 `bk_port_dcache_invalidate(frame->data, frame->alloc_size)`。
    - 当 CPU 修改了 `frame->data`（如软件绘制 OSD 水印）且随后需交给 LCD DMA 显示时，调用方必须调用 `bk_frame_sync_for_device(frame)`（内部执行 `dcache_clean`）。
- **并发与帧耗尽背压契约**：
  - `bk_frame_retain(frame)` 与 `bk_frame_release(frame)` 内部使用 C11 原子操作（或关中断自旋锁）维护引用计数 `ref_cnt`，**保证跨任意线程安全**。
  - **帧池耗尽与丢帧策略**：创建帧池时需指定 `bk_frame_exhaust_policy_t`：
    - `BK_FRAME_POLICY_WAIT_TIMEOUT`：在 `timeout_ms` 内等待消费者释放帧，超时返回 `NULL` 并计入 `alloc_drop_count`。
    - `BK_FRAME_POLICY_DROP_OLDEST`：适用于实时预览队列，当队列满时自动 `release` 队列中最旧的未消费帧以腾出槽位。
  - **反初始化契约**：若调用 `bk_frame_pool_destroy(pool)` 时仍有帧的 `ref_cnt > 0`（外部消费者尚未释放），函数必须拒绝销毁并返回 `BK_ERR_BUSY`，防止野指针踩内存。

```c
// components/bk_video_frame_pool/include/bk_video_frame_pool.h
typedef struct {
    uint8_t           *data;          // 32-byte aligned PSRAM buffer
    size_t             alloc_size;    // 对齐后的总容量
    size_t             valid_len;     // 当前帧实际有效字节数
    uint16_t           width;
    uint16_t           height;
    bk_pixel_fmt_t     format;        // JPEG / H264 / YUV422 / RGB565
    uint64_t           timestamp_us;  // 采集单调时钟时间戳
} bk_video_frame_t;

bk_err_t bk_frame_pool_create(const bk_frame_pool_cfg_t *cfg, bk_frame_pool_handle_t *out_pool);
bk_err_t bk_frame_pool_destroy(bk_frame_pool_handle_t pool); // 若存在未归还帧则返回 BK_ERR_BUSY

bk_video_frame_t *bk_frame_alloc(bk_frame_pool_handle_t pool, uint32_t timeout_ms);
void bk_frame_retain(bk_video_frame_t *frame);               // 原子递增引用计数 (跨线程安全)
void bk_frame_release(bk_video_frame_t *frame);              // 原子递减，归零时自动归还 pool
void bk_frame_sync_for_device(bk_video_frame_t *frame);      // CPU写后执行 D-Cache Clean
void bk_frame_sync_for_cpu(bk_video_frame_t *frame);         // DMA写后执行 D-Cache Invalidate
```

### 4.5 `bk_ota`（发布级 OTA 安全与恢复契约 —— 解决 P0-4）

`bk_ota` 绝不只是一个简单的 `bk_ota_start()` 转发器，必须实现完整的升级前后状态机、安全校验与故障恢复契约（列为 P0 级验收标准）：

1. **前置媒体管线协同与拒绝策略（Pre-OTA Pipeline Quiesce）**：
   - OTA 启动下载前，通过 `bk_core_event` 发送同步请求挂起 `bk_audio_session` 与 `bk_video_frame_pool`，以释放 PSRAM 缓冲区和 Flash 写入带宽。
   - **挂起失败契约**：若媒体组件在 `cfg->quiesce_timeout_ms`（默认 3000ms）内未能安全停止或返回 `BK_ERR_BUSY`，**`bk_ota_start()` 必须立即中止升级并返回 `BK_ERR_OTA_MEDIA_BUSY`**，严禁在音视频 DMA 高负载运行时强行擦写 Flash 导致系统看门狗超时崩溃。
2. **镜像完整度、签名验证与防回滚（Signature & Anti-Rollback）**：
   - 下载流在写入 OTA 目标分区时，按 4KB 扇区计算滚动哈希，并在收尾阶段验证整个固件包的 **SHA-256 摘要**与**非对称数字签名（ECDSA P-256 / RSA-2048，公钥内置于只读 Flash/Bootloader）**。签名不合法立即擦除 OTA 分区头部并返回 `BK_ERR_OTA_SIGNATURE_INVALID`。
   - **Anti-Rollback（防降级）**：对比固件头部中的 `security_version` 与当前芯片 EFUSE/安全存储中的最小允许安全版本号；若新固件安全版本号更低，拒绝升级并返回 `BK_ERR_OTA_ROLLBACK_DETECTED`。
3. **下载中断、断电恢复与重试契约（Power-Loss & Resume）**：
   - 每成功写入并校验完一个 64KB 块，在 `bk_storage` 的 `"ota_state"` 命名空间记录当前固件 URL、ETag/SHA256 前缀与已验证写入偏移量 `verified_offset`。
   - 若下载中途断网或断电重启，下次调用 `bk_ota_start()` 时若检测到相同固件标识且已写入区的 SHA-256 校验通过，则使用 HTTP `Range: bytes=<verified_offset>-` 断点续传；若校验不匹配或固件标识改变，则从 `0` 重新擦写。
4. **双分区原子切换与启动自检回滚（Bootloader Rollback Confirmation）**：
   - 新固件写入并校验通过后，将分区状态标记为 `PENDING_VERIFY` 并重启。
   - 新固件首次启动后，必须在完成核心服务初始化与联网自检后显式调用 `bk_ota_mark_app_valid()` 将状态置为 `VALID`；若新固件在启动过程中崩溃、卡死触发硬件看门狗或未在规定窗口内确认，Bootloader 自动回退至上一已知良好版本并上报 `BK_EVENT_OTA_ROLLED_BACK`。

### 4.6 `bk_ai_agent`（传输层与多协议状态机严格解耦 —— 解决 P0-3）

修正 Rev 1 中将“小智协议”与“OpenAI Realtime”混为一谈的问题，明确将**传输通道（Transport）**与**会话协议（Protocol Engine）**解耦：

1. **架构分层**：
   - **传输层 (`transport_websocket`)**：仅负责标准 RFC 6455 WebSocket over TLS (WSS) 的连接建立、自定义 HTTP Headers 注入、Text Frame / Binary Frame 发送接收、Ping/Pong 心跳检测与断线通知。
   - **协议层 1 (`protocol_xiaozhi` —— Phase 2 优先实现)**：
     - **许可证合规说明**：参考 [78/xiaozhi-esp32 WebSocket 协议文档](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)（上游仓库为 **MIT License**）。本框架根据公开协议文档编写净室实现（Clean-room C implementation），并在组件 `NOTICE` 与依赖清单中完整声明协议来源与许可证信息。
     - **独立协议状态机**：
       1. 连接成功后发送 `{"type": "hello", "version": 1, "transport": "websocket", "audio_params": {"format": "opus", "sample_rate": 16000, "channels": 1, "frame_duration": 60}}`。
       2. 校验服务端返回的 `hello` 响应帧，提取会话 `session_id` 与服务端音频参数后才进入 `READY` 状态。
       3. 音频上行/下行采用 **WebSocket Binary Frame** 直接承载裸 Opus 帧（或带协议版本指定的头部）。
       4. 语音状态控制（`listen: start/stop/detect`、`abort`）、TTS 状态（`tts: start/sentence_start/stop`）、情感表情（`llm: emotion`）及端侧能力调用（`iot` / `mcp` JSON-RPC 2.0 消息）均通过 **WebSocket Text Frame (JSON)** 独立路由。
   - **协议层 2 (`protocol_openai_realtime` —— 远期可选扩展)**：
     - 采用完全独立的事件驱动模型（`session.update`、`input_audio_buffer.append` Base64 JSON 帧、`response.audio.delta` 等），不与 `protocol_xiaozhi` 混用状态机。
   - **商业插件层 (`plugin_agora`)**：
     - 独立封装 Agora RTC/RTM SDK，仅在启用 `bk7259_robot` 商业插件配置时参与编译。
2. **每个协议模块的独立验收门禁**：
   - 必须各自提供：1) Wire-format 报文规范文档；2) 正常/异常握手单元测试；3) 断线重连与会话恢复测试；4) 本地 Mock Server 端到端回环测试；5) 独立许可证与第三方依赖清单（如 `cJSON`、`libopus`）。

---

## 五、 四级验证门禁体系（4-Tier Verification Gate —— 解决 P1-9）

不再声称“POSIX 模拟器可以 100% 验证组件”，而是根据验证对象的物理属性，建立严格的四级递进门禁：

| 门禁级别 | 执行环境 | 可验证的内容（能力边界） | 明确无法验证的内容（禁止越界声称） | 准出通过标准 |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 1: Host 行为门禁**<br>(`posix_sim` + CTest/PyTest) | Linux x86_64/ARM64 主机 (GCC/Clang + ASan/UBSan/TSan) | - `bk_core_event` 订阅/发布顺序与并发锁<br>- `bk_storage` 序列化、CRC 损坏恢复、安全擦除零化逻辑<br>- `bk_net` 退避重连计时与状态机迁移<br>- `bk_video_frame_pool` 原子引用计数与耗尽策略<br>- `bk_audio_mixer` 定点混音与衰减算术<br>- `protocol_xiaozhi` JSON/Binary/MCP 报文解析与握手状态机 | **不能证明**：AP/CP 核间 IPC、PSRAM/DMA Cache 一致性、真实 Wi-Fi/BLE 射频、硬件 AEC 声学效果、链接脚本与 `all-app.bin` 打包。 | ASan（内存越界/泄漏）与 TSan（数据竞争）零告警，全部分支单测 100% PASS。 |
| **Tier 2: SDK 编译与尺寸门禁**<br>(`bk7259_v4` Cross-Compile) | Armino 工具链 + 锁定 SHA 的 `bk_avdk_smp` (`release/v4.0.1.11`) | - 外部组件在 AP/CP CMake/Kconfig 中的注入编译<br>- Kconfig 开关关闭时组件不进入最终 ELF 符号表<br>- AP + CP 双侧构建并成功生成 `all-app.bin`<br>- 静态 Flash / SRAM / PSRAM 段占用审计 | **不能证明**：运行期时序、外设实际通信、DMA 传输正确性与整机功耗。 | `bk7259_minimal` 与 `bk7259_robot` 均成功生成 `all-app.bin`；关闭可选组件后 `nm`/`objdump` 确认无残留符号。 |
| **Tier 3: 故障注入与压力门禁**<br>(Mock Peripheral / HIL) | 板级回环测试桩 / 本地 Mock Server | - 弱网/断网重连、DNS 超时、TLS 过期证书拒绝测试<br>- OTA 下载中途断电/断网恢复、坏签名镜像拒绝测试<br>- 帧池耗尽背压与下行音频满缓冲恢复测试<br>- WebSocket Mock Server 高频打断（Abort）压力测试 | **不能证明**：真实光学/声学物理指标（如双麦信噪比、镜头畸变）。 | 连续 1000 次状态切换与故障注入无死锁、无句柄泄漏、PSRAM 水位回归基线。 |
| **Tier 4: 真实板卡验收门禁**<br>(`bk7259_robot` Hardware Gate) | `bk7259_robot` 实机开发板 | - 真实上电 Boot 与双核串口日志输出<br>- BLE/SoftAP 真实配网与路由器重连<br>- 实机 Mic 采集 + AEC + 扬声器播放与混音<br>- 实机 LCD 刷屏与 MIPI CSI 摄像头预览<br>- 实机 OTA 升级与强制回滚验证 | —（最终发布门禁） | 逐项填写并通过 `docs/release-checklist.md` 实机验收表，方可打 `v0.1.0` 正式 Tag。 |

---

## 六、 修正后的增量实施顺序（Evidence-Driven Roadmap）

严格采纳 Codex 评审建议，按**“先闭合 Task 0 编译证据 -> 最小公共层 -> 单一媒体能力 -> 安全 OTA -> 独立小智协议 -> 最后再考虑 BK7258 扩展”**的顺序推进：

### Step 0：闭合 BK7259 Task 0 构建与硬件事实闭环（前置阻塞项）
- 在具备锁定版 `bk_avdk_smp` (`24cce68adc...`) 与交叉编译工具链的环境中运行 `tools/spikes/bk7259_component_probe/probe.sh --build`。
- 验证空外部组件在 AP/CP 中的接入编译、Kconfig 关闭裁剪以及最终 `all-app.bin` 生成，将 `platforms/bk7259_*.yaml` 中的 `not_run_toolchain_unavailable` 更新为实测证据。
- 核对 `bk7259_robot` 源码中的实际引脚定义，明确区分 `verified` 与 `unverified` 硬件能力表。

### Step 1：实现最小公共层与最小联网闭环（M1）
- 实现 `./tools/bk`（`doctor`、`fetch`、`env`）及 manifest 严格校验。
- 实现最小核心组件：`bk_common`、`bk_port`（`bk7259_v4` + `posix_sim`）、`bk_core_event`、`bk_storage`（含敏感凭证加密与安全擦除契约）、`bk_board`（仅暴露已核实的 `bk7259_robot` 元数据，受控 CP 模板校验）和 `bk_net`（含 TLS/SNTP 安全契约）。
- 交付 `examples/01_wifi_provision` 与 `examples/02_lan_device`，通过 Tier 1（Host 单测）与 Tier 2（`bk7259_minimal` `all-app.bin` 编译验证）。

### Step 2：增量实现单一媒体机制与基础外设组件（M2-A）
- **不同时开火所有媒体与 AI 模块**。先独立实现并验证 `bk_button`、`bk_led_effect`，以及一个核心媒体组件（先落地 `bk_audio_session` 的单路播放/采集与背压管理，再独立落地 `bk_video_frame_pool` 的 PSRAM 引用计数与 Cache 同步）。
- 交付 `examples/03_audio_player` 与 `examples/04_camera_stream`，通过 Tier 1~Tier 4 验证。

### Step 3：实现发布级安全 OTA 组件（M2-B）
- 实现 `bk_ota`：落地媒体管线挂起协同（挂起失败拒绝升级）、签名校验、Anti-rollback、64KB 块校验断点续传以及 Bootloader 启动自检确认（`bk_ota_mark_app_valid`）。
- 在 `bk7259_robot` 实机上完成正常升级、坏签名拒绝、断电续传与未确认自动回滚四项硬件验收。

### Step 4：实现独立的小智协议适配器与机器人参考工程（M3）
- 实现 `bk_ai_agent/transport_websocket` 与 `bk_ai_agent/protocol_xiaozhi`（独立 wire-format 文档、握手/重连/MCP 测试及 MIT 协议来源声明）。
- 实现可选插件接口 `plugins/plugin_agora`，并将机器人专用策略（`bk_motion`、`bk_robot_face`）放入 `product_components/`，完成 `products/bk7259_robot` 在“纯开源小智模式”与“Agora 插件模式”下的实机验收，发布 `v0.1.0`。

### Step 5（Phase 3 远期规划）：BK7258 (`release/v3.1.1.x`) 跨代适配与 Auto-CP 演进
- 在 `v0.1.0` 发布后，再启动 `bk7258_v3` 后端适配、新增 `platforms/bk7258_minimal.yaml`，并根据已验证的多板卡经验评估 CP 配置受控生成工具。
