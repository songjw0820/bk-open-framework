# BK Open Framework 架构深度评审与重构方案（对标乐鑫开源生态 · 供架构/Codex 评审版）

- **文档路径**：`docs/reviews/2026-11-13-architecture-review-and-espressif-comparison.md`
- **关联现有文档**：
  - 现有设计规范：[docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](file:///home/antigravity/songjw0820/bk-open-framework/docs/superpowers/specs/2026-11-12-bk-open-framework-design.md)
  - 现有实施计划：[docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md](file:///home/antigravity/songjw0820/bk-open-framework/docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md)
  - 平台探针证据：[docs/platforms/bk7259_v4.0.1.11.md](file:///home/antigravity/songjw0820/bk-open-framework/docs/platforms/bk7259_v4.0.1.11.md)

---

## 一、 现状诊断（Current State Diagnosis）

### 1.1 仓库与工程现状事实核查
1. **代码成熟度为零（处于 Task 0 静态探针阶段）**：
   - 当前仓库共有 10 个文件，仅包含设计文档、实施计划、依赖审计清单、2 个平台 YAML（`bk7259_minimal.yaml`、`bk7259_robot.yaml`）以及 1 个静态目录检查脚本（`tools/spikes/bk7259_component_probe/probe.sh`）。
   - `components/`、`boards/`、`examples/`、`products/` 均尚未创建。
   - 构建验证状态为 `not_run_toolchain_unavailable`（尚未在带交叉编译工具链与锁定 SDK 的环境中实测 AP/CP 编译及 `all-app.bin` 打包）。
2. **现有架构设计的合理之处（应保留）**：
   - **上游 SDK 与 Solution 外置锁定**：通过 manifest 锁定 `bk_avdk_smp` 与 `bk_solution_ai` 的同名 release tag 及精确 commit SHA，避免将数 GB 的底层 SDK 复制进框架仓。
   - **商业闭源依赖剥离**：将声网（Agora）RTC/RTM 与特定商业云 SDK 从核心框架剥离为可选插件，确保核心仓库满足 `Apache-2.0` 开源合规。
   - **最小 Profile 解耦**：`bk7259_minimal` 仅依赖 `bk_avdk_smp` SDK，不强耦合 `bk_solution_ai` 机器人工程。

### 1.2 现有设计的四大核心缺陷（必须修正）

| 编号 | 缺陷类别 | 具体问题表现（基于现有 Spec 与 Plan） | 对模组应用开发者的实际影响 |
| :--- | :--- | :--- | :--- |
| **D1** | **组件空壳化（Thin Wrapper Syndrome）** | 实施计划 Task 4~8 将 `bk_board`、`bk_media`、`bk_ui`、`bk_ota`、`bk_peripheral` 全部定义为对 SDK 的“薄封装（thin adapters）”。例如 `bk_board` 仅提供 `has_capability()` 和 `get_pin()`；`bk_ota` 仅包装 `bk_ota_start()`；剥离 Agora 插件后 `bk7259_robot` 仅承诺“开机与基础联网”。 | 组件沦为“给 SDK 函数改名的二传手”，没有解决任何复杂状态机、音视频混音管线、外设消抖/灯效/舵机编排、开源 AI 对话等实质问题，开发者直接用官方 SDK 反而更直接。 |
| **D2** | **未屏蔽 AP/CP 双核异构复杂性** | BK7259/BK7258 采用 AP + CP 双子系统（独立 `ap/CMakeLists.txt` 与 `cp/CMakeLists.txt`，甚至 CP UART 引脚位于 `cp/config/bk7259/usr_gpio_cfg.h`）。现有设计未定义组件在 AP/CP 间的归属，也未屏蔽 CP 工程。 | 模组开发者写一个简单应用需同时理解并修改 `ap/` 与 `cp/` 两套工程及核间通信，上手门槛远高于乐鑫 ESP32 单工程 SMP 模式。 |
| **D3** | **缺乏跨代 SDK 磨平层，无法同时支撑 BK7258 与 BK7259** | Spec 第 8 节明确指出 BK7258 基于 `release/v3.1.1.x`（三核 AMP，`media_app_*` 体系），BK7259 基于 `release/v4.0.1.x`（AP/CP SMP，新一代 AVDK 与 NPU）。但现有组件直接建立在 `v4.0.1.11` 之上，无底层 Porting 隔离层。 | 第一阶段为 BK7259 编写的组件会直接耦合 `v4.0.1` 头文件与原语，后续扩展 BK7258 时将导致组件层大面积返工甚至分裂为两套代码。 |
| **D4** | **混同“模组（Module）”与“底板（Board）”** | 现有设计只有 `boards/bk7259_robot` 整机板概念，未区分核心模组（Flash/PSRAM 容量、线序、分区表、RF 参数）与客户扩展底板（LCD、Camera、Codec、按键 GPIO）。 | 模组客户采购标准 BK7258/BK7259 模组自研底板时，无法通过简单配置复用模组级 Flash/PSRAM/分区设置。 |

---

## 二、 深度对标：BK Open Framework vs 乐鑫（Espressif）开源生态

对比乐鑫 `ESP-IDF` + `ESP-BSP` + `ESP-ADF` + `ESP-IoT-Solution` + `xiaozhi-esp32`（小智开源生态），差距不在底层芯片能力（BK7258/BK7259 在单芯片音视频编解码、硬件 AEC、大 PSRAM 与 NPU 集成度上极具优势），而在**中间件厚度与开发者工程体验**：

| 对标维度 | 乐鑫开源生态标杆 | 当前 `bk-open-framework` 规划 | 建议重构目标 |
| :--- | :--- | :--- | :--- |
| **1. 编程视图与多核屏蔽** | 同构 SMP 单工程，一个 `app_main()` 统一调用 Wi-Fi / BLE / Audio / LVGL。 | 裸露 AP (`ap/CMakeLists.txt`) 与 CP (`cp/CMakeLists.txt`) 双工程。 | **AP 单核开发视图 + CP 配置自动生成（Auto-CP）**：应用开发者只写 AP 侧业务代码，CP 侧引脚/电源/IPC 配置由构建工具根据 `board.yaml` 自动生成。 |
| **2. 板级支持包（BSP）交付深度** | **`ESP-BSP` 句柄级交付**：`bsp_display_start()` 直接完成电源、总线、LCD 驱动、LVGL 刷屏任务初始化并返回 `lv_disp_t*`；音频直接返回 `esp_codec_dev_handle_t`。 | **`bk_board` 引脚级交付**：仅提供 `bk_board_get_pin()` 返回 GPIO 编号与 `bk_board_has_capability()`。 | **升级为 `modules/` + `boards/` 两级硬件工厂**：内置常用 LCD/Camera/Audio 初始化序列与 PA 防爆音时序，向业务层直接交付可读写设备句柄。 |
| **3. 外设组件实质功能** | **`ESP-IoT-Solution`**：`iot_button`（消抖+多击/长按/组合键状态机）、`led_indicator`（呼吸/闪烁优先级抢占队列）、`servo`（平滑插补）。 | **`bk_peripheral`**：仅做板级 capability 检查与底层 GPIO/PWM 薄包装。 | 下沉实质状态机：内置**多击按键引擎（支持 GPIO 与 ADC 分压按键）**、**优先级灯效状态机**、**多自由度舵机余弦插补与关键帧动作播放器**、**电池 OCV 电量滤波与低电休眠管理**。 |
| **4. 音视频中间件厚度** | **`ESP-ADF` / `GMF`**：完整的 Audio Pipeline、Element 流转、AEC/VAD 声学前端、多路混音器（Ducking/Mixing）、HTTP/Opus 流式播放器。 | **`bk_media`**：薄包装 SDK 音视频接口，无混音管理与跨模块帧生命周期管理。 | 提供**双流音频焦点与混音管线**（实时双向语音流 + 本地提示音自动压低背景音混音）与**引用计数零拷贝 PSRAM 视频帧池**（一路采集同时供预览、推流与 AI 识图）。 |
| **5. 开源端云 AI 链路** | **`xiaozhi-esp32` / `ESP-Box`**：纯开源 `WebSocket / MQTT+UDP + Opus + JSON` 端到端语音/视觉大模型交互，零商业授权费即可跑通完整产品。 | 强依赖 `bk_solution_ai` 中的声网（Agora）闭源插件；无插件时仅能开机联网。 | 新增 **`bk_ai_agent` 开源多模态连接组件**：内置 Apache-2.0 纯开源 `WebSocket + Opus + JPEG` 协议栈（兼容小智/OpenAI Realtime 协议），将 Agora 作为可选 `bk_ai_transport_t` 插件。 |
| **6. 跨芯片与单测体系** | 统一 `esp_hw_support` + Linux Host POSIX 模拟器（无需开发板即可在 CI 运行组件单测）。 | 直接依赖 `v4.0.1.11` 头文件，无 SDK 无法编译。 | 新增最底层 **`bk_port`**：提供 `bk7258_v3`、`bk7259_v4`、`posix_sim` 三套后端，保证所有上层组件在无 SDK/无硬件的 CI 环境下也能 100% 编译并跑通状态机单元测试。 |
| **7. CLI 工具链体验** | `idf.py create-project / set-target / menuconfig / build / flash / monitor` 一体化。 | `./tools/bk` 仅规划 `doctor / fetch / env`，构建仍需手写 `make -C $SDK_DIR ...`。 | 扩展 `./tools/bk` 支持 `create-project / set-board / menuconfig / build / flash / monitor` 及双核日志分色与 Crash `addr2line` 解析。 |

---

## 三、 重构后的五层架构与边界契约（Architecture & Contracts）

### 3.1 架构分层图

```text
+-----------------------------------------------------------------------------------+
| L5: 应用示例与产品模板层 (仅编写 AP 侧单核代码，不感知 CP 核与芯片型号宏)           |
|     examples/01..06  |  products/bk7259_robot  |  products/bk7258_voice_assistant |
+-----------------------------------------------------------------------------------+
                                         |
+-----------------------------------------------------------------------------------+
| L4: 端云 AI 交互与可选商业插件层 (Connectors & Plugins)                           |
|     components/bk_ai_agent (内置开源 WebSocket+Opus+JPEG 协议栈 + MCP 工具分发)   |
|     plugins/agora_rtc (可选闭源插件)  |  plugins/volcengine_rtc (可选闭源插件)    |
+-----------------------------------------------------------------------------------+
                                         |
+-----------------------------------------------------------------------------------+
| L3: 实质性业务中间件层 (Substantive Middleware Components - 100% 芯片无关)        |
|     bk_core       : 跨核事件总线(bk_event) / KV持久化配置(bk_kv) / 内存与健康守护 |
|     bk_net        : BLE+SoftAP配网协议栈 / 多AP按信号漫游 / 指数退避 / SNTP门控   |
|     bk_media      : 实时语音流(AEC/VAD/Opus) + 提示音闪避混音器 / 零拷贝视频帧池  |
|     bk_ui         : LVGL线程安全锁 / 顶部状态栏组件 / 参数化机器人双目表情引擎    |
|     bk_peripheral : GPIO+ADC多击按键状态机 / 优先级灯效队列 / 舵机关键帧 / 电池管理|
|     bk_ota        : 媒体管线自动挂起 / HTTPS断点续传 / 进度节流事件 / 整包校验    |
+-----------------------------------------------------------------------------------+
                                         |
+-----------------------------------------------------------------------------------+
| L2: 模组与板级支持包工厂层 (Module & BSP Factory - bk_board + Auto-CP)            |
|     modules/ (bk7258_8m8m, bk7259_16m16m: 分区表 / PSRAM配置 / RF参数)            |
|     boards/  (bk7258_devkit, bk7259_robot: 实例化 Display/Audio/Cam + 生成 CP 配置)|
+-----------------------------------------------------------------------------------+
                                         |
+-----------------------------------------------------------------------------------+
| L1: 跨代 SDK 与跨芯片移植层 (bk_port)                                             |
|     port_bk7258_v3 (Armino v3.1.1.x) | port_bk7259_v4 (SMP v4.0.1.x) | posix_sim  |
+-----------------------------------------------------------------------------------+
```

### 3.2 AP/CP 双核职责边界与 Auto-CP 机制（解决多核痛点）

为了消除模组开发者的双核开发负担，框架强制执行以下 **AP/CP 架构规则**：

1. **组件运行域（Execution Domain）**：
   - 除 `bk_port` 的 CP 侧 IPC 桩代码外，`bk_core`、`bk_net`、`bk_media`、`bk_ui`、`bk_peripheral`、`bk_ota`、`bk_ai_agent` 的业务 API **100% 运行在 AP 核**。
   - 每个组件目录下的 `bk_component.yaml` 必须显式声明 `execution_domain: ap | cp | both`。
2. **CP 侧配置自动生成（Auto-CP Generator）**：
   - 开发者**只在 `boards/<board_name>/board.yaml` 中声明硬件引脚与外设归属**。
   - 运行 `./tools/bk set-board <board_name>` 或 `./tools/bk build` 时，`tools/bk_cp_autogen.py` 自动从 `board.yaml` 生成：
     - CP 侧引脚配置头文件（如 `usr_gpio_cfg.h`）
     - AP 侧板级描述符 C 代码（`bk_board_generated.c`）
     - 对应模组 Flash 大小的分区表（`partitions.csv`）
   - 除非客户需要定制底层 Wi-Fi 低功耗保活协议，否则**无需手动编辑任何 `cp/` 目录文件**。

---

## 四、 核心组件实质性功能与 C API 契约设计

以下定义各组件必须实现的**实质性机制**与核心 C 接口（取代原有的空壳薄封装设计）：

### 4.1 `bk_port`（跨 BK7258 `v3.1.1` / BK7259 `v4.0.1` / `posix_sim` 磨平层）

**实质功能**：
- 隔离 RTOS 线程/队列/互斥锁/高分辨率定时器差异。
- 统一管理 SRAM 与 PSRAM 内存分配，内置按 `tag` 统计的内存水位与泄漏检测表。
- 隔离底层 Wi-Fi/BLE/Audio/Camera/Display 驱动在不同版本 Armino SDK 中的头文件与签名差异。

```c
// components/bk_port/include/bk_port_mem.h
typedef enum {
    BK_MEM_CAP_DEFAULT = 0,
    BK_MEM_CAP_SRAM    = (1 << 0), // 内部高速 SRAM（用于中断、线程栈、关键控制块）
    BK_MEM_CAP_PSRAM   = (1 << 1), // 外部大32MB/16MB/8MB PSRAM（用于音视频帧池、UI缓冲）
    BK_MEM_CAP_DMA     = (1 << 2), // DMA 对齐内存
} bk_mem_cap_t;

void *bk_port_malloc(size_t size, uint32_t caps, const char *tag);
void  bk_port_free(void *ptr);
bk_err_t bk_port_mem_get_stats(bk_mem_stats_t *out_stats); // 获取 SRAM/PSRAM 实时剩余与历史最低水位
```

### 4.2 `bk_core`（异步事件总线 + 持久化 KV 配置中心 + 系统健康守护）

**实质功能**：
1. **带优先级的异步事件总线 (`bk_event`)**：内部维护独立的工作线程与环形消息队列，支持按 `domain + event_id` 订阅，支持从 ISR 或跨核 IPC 投递事件，防止发布者阻塞在慢速回调上。
2. **RAM 缓存 + 延迟落盘 KV 配置中心 (`bk_kv`)**：解决音视频流传输期间频繁擦写 Flash 导致 XIP 挂起和音频爆音的问题，支持脏数据定时合并落盘（Debounce Flush）与 CRC32 完整性校验。
3. **系统看门狗与健康监控 (`bk_health`)**：周期采样 PSRAM 剩余水位、AP/CP 核间心跳，触发低内存预警事件 `BK_EVENT_CORE_LOW_MEMORY`。

```c
// components/bk_core/include/bk_kv.h
bk_err_t bk_kv_init(const char *partition_name);
bk_err_t bk_kv_set_blob(const char *ns, const char *key, const void *data, size_t len);
bk_err_t bk_kv_get_blob(const char *ns, const char *key, void *out_buf, size_t *inout_len);
bk_err_t bk_kv_set_u32(const char *ns, const char *key, uint32_t val);
bk_err_t bk_kv_get_u32(const char *ns, const char *key, uint32_t *out_val, uint32_t default_val);
bk_err_t bk_kv_commit(void); // 立即将脏缓存刷入 Flash
```

### 4.3 `bk_board`（模组 + 底板两级 BSP 工厂，对标 `ESP-BSP`）

**实质功能**：
1. **两级 YAML 模型**：
   - `modules/<module>.yaml`：描述模组芯片（`BK7258` 或 `BK7259`）、Flash/PSRAM 容量与线序（如 `16MB_Flash_16MB_OPI_PSRAM`）、分区表模板。
   - `boards/<board>/board.yaml`：引用 `module`，声明音频（Mic 通道、AEC 硬件回采、PA 使能脚及上电防爆音延迟 ms）、显示（控制器型号、分辨率、SPI/RGB/MIPI 接口、背光 PWM 脚）、摄像头（DVP/MIPI、Sensor 型号）、按键与传感器引脚。
2. **句柄级初始化工厂**：不再只返回 GPIO 号，而是直接完成总线与驱动初始化并返回统一抽象句柄。

```c
// components/bk_board/include/bk_board.h
bk_err_t bk_board_init(void); // 初始化电源域、引脚复用与板级基础总线(I2C/SPI)
const bk_module_info_t *bk_board_get_module_info(void);
const bk_board_info_t  *bk_board_get_info(void);
bool bk_board_has_capability(bk_board_cap_t cap);

// 句柄级外设工厂（内部处理上电时序、PA Mute 防爆音、分辨率适配）
bk_err_t bk_board_audio_init(bk_audio_dev_handle_t *out_audio);
bk_err_t bk_board_display_init(bk_display_handle_t *out_disp);
bk_err_t bk_board_camera_init(bk_camera_handle_t *out_cam);
```

### 4.4 `bk_net`（双模配网协议栈 + 多 AP 漫游记忆 + 退避重连与 SNTP 门控）

**实质功能**：
1. **标准 BLE GATT / SoftAP 配网状态机**：内置周边 Wi-Fi 扫描结果分页读取、SSID/Password 写入、配网失败细分原因上报（`AUTH_FAIL` / `AP_NOT_FOUND` / `DHCP_TIMEOUT`），无需每个项目重写 BLE GATT 表。
2. **多路由器凭证 LRU 存储与信号择优漫游**：在 `bk_kv` 的 `"wifi_creds"` 命名空间下保存最多 5 组 AP 信息；启动扫描后优先连接信号最强（RSSI 最大）的已知 AP。
3. **指数退避重连与 Cloud-Ready 事件门控**：断线后按指数退避（1s, 2s, 4s, 8s, 最大 30s）自动重连；连上 IP 后自动触发 SNTP 时间同步，完成后发布 `BK_NET_EVENT_CLOUD_READY`（确保后续 HTTPS/WSS 证书有效期校验必过）。

```c
// components/bk_net/include/bk_net.h
typedef enum {
    BK_NET_STATE_IDLE = 0,
    BK_NET_STATE_PROVISIONING,   // 正在配网中
    BK_NET_STATE_CONNECTING,     // 正在连接 AP
    BK_NET_STATE_IP_READY,       // 已获取 IP，等待时间同步/外网探测
    BK_NET_STATE_CLOUD_READY,    // IP + SNTP 就绪，可发起 TLS/云连接
    BK_NET_STATE_DISCONNECTED,   // 断网退避重连中
} bk_net_state_t;

bk_err_t bk_net_init(const bk_net_config_t *cfg);
bk_err_t bk_net_start(void); // 若存在已保存AP则自动择优连接，否则可配置为自动进入配网
bk_err_t bk_net_start_provisioning(bk_prov_mode_t mode, uint32_t timeout_ms);
bk_err_t bk_net_save_ap_credential(const char *ssid, const char *password);
bk_net_state_t bk_net_get_state(void);
```

### 4.5 `bk_media`（双流音频焦点/混音管线 + 零拷贝视频帧缓冲池）

**实质功能**：
1. **双流音频焦点与混音器（Voice Stream + Tone Player）**：
   - 实时采集链路：`Mic PCM -> AEC/NS/VAD -> Opus/G711/PCM 编码回调`。
   - 实时播放与本地提示音混音：支持在进行远端实时语音播放时，插入本地提示音（如按键音、状态提示音），支持 **Ducking（自动将语音流音量衰减 -12dB 并叠加提示音 PCM）** 或 **Preempt（暂停语音流，播完提示音后恢复）**。
2. **引用计数零拷贝视频帧池（`bk_frame_pool`）**：
   - 在 PSRAM 中预分配固定数量的帧槽位（避免运行期大块内存碎片）。
   - 摄像头采集一帧后生成 `bk_video_frame_t*`（初始引用计数为 1），同时投递给 UI 预览消费者和网络推流消费者时调用 `bk_frame_retain()`，各方消费完毕调用 `bk_frame_release()`，归零时自动归还帧池。

```c
// components/bk_media/include/bk_media.h
typedef enum {
    BK_TONE_MIX_DUCKING = 0, // 压低远端语音音量并混音播放提示音
    BK_TONE_MIX_PREEMPT = 1, // 抢占打断当前音频，播放完毕后恢复
} bk_tone_mix_policy_t;

bk_err_t bk_media_init(const bk_media_config_t *cfg);
// 语音双向流控制
bk_err_t bk_media_voice_start(const bk_voice_pipeline_cfg_t *cfg, bk_voice_uplink_cb_t on_encoded_frame, void *ctx);
bk_err_t bk_media_voice_push_downlink(const uint8_t *encoded_data, size_t len, uint32_t timestamp_ms);
// 本地提示音播放（支持混音策略）
bk_err_t bk_media_play_tone(const uint8_t *audio_buf, size_t len, bk_audio_fmt_t fmt, bk_tone_mix_policy_t policy);

// 零拷贝视频帧池
bk_err_t bk_frame_pool_create(size_t frame_Count, size_t max_frame_bytes, bk_frame_pool_handle_t *out_pool);
bk_video_frame_t *bk_frame_alloc(bk_frame_pool_handle_t pool, uint32_t timeout_ms);
void bk_frame_retain(bk_video_frame_t *frame);
void bk_frame_release(bk_video_frame_t *frame);
```

### 4.6 `bk_peripheral`（多击按键引擎 + 优先级灯效队列 + 舵机关键帧动作引擎 + 电池管理）

**实质功能**：
1. **`bk_button`**：支持独立 GPIO 按键与 ADC 分压多按键；内置 20ms 消抖、单击、双击、长按触发（如 1.5s）、超长按恢复出厂（如 5s）及双键组合检测，直接向 `bk_event` 发布标准按键事件。
2. **`bk_led_effect`**：支持多优先级图层（Layer 0: 待机呼吸 < Layer 1: AI 对话律动 < Layer 2: 配网闪烁 < Layer 3: 低电/故障告警）。高优先级灯效激活时自动挂起低优先级，退出后自动恢复。
3. **`bk_motion`**：针对机器人/云台舵机或电机，提供基于关键帧时间轴（Keyframe Timeline）的余弦加减速平滑插补引擎，防止舵机瞬间满速突变导致电源跌落重启。
4. **`bk_power`**：ADC 电压中值+滑动均值滤波、充电引脚状态去抖、OCV 电池电量曲线映射。

```c
// components/bk_peripheral/include/bk_peripheral.h
// 1. 按键状态机
bk_err_t bk_button_register(const bk_button_config_t *cfg, bk_button_handle_t *out_btn);
// 2. 优先级灯效引擎
bk_err_t bk_led_set_effect(uint8_t led_id, bk_led_priority_t prio, const bk_led_pattern_t *pattern);
bk_err_t bk_led_clear_effect(uint8_t led_id, bk_led_priority_t prio);
// 3. 机器人舵机关键帧序列播放（非阻塞平滑插补）
typedef struct {
    uint8_t  channel;
    int16_t  target_angle_deg; // -90 ~ +90
    uint32_t duration_ms;      // 平滑过渡时长
} bk_motion_keyframe_t;
bk_err_t bk_motion_play_sequence(const bk_motion_keyframe_t *frames, size_t count, bool loop);
```

### 4.7 `bk_ai_agent`（新增：开源端云多模态交互组件 + 可插拔商业 RTC 接口）

**实质功能**：
- 解决“剥离 Agora 后机器人无法工作”的致命断层。
- 定义统一的 `bk_ai_transport_ops_t` 接口：
  - **内置默认开源实现 (`bk_ai_transport_websocket`)**：基于 Apache-2.0 实现标准 WebSocket 双向音频（Opus/PCM）+ 视觉抽帧（JPEG）+ 状态/工具调用 JSON 协议（兼容开源社区主流的小智协议及 OpenAI Realtime 风格网关）。
  - **可选商业插件 (`plugins/agora_rtc`)**：实现同一个 `bk_ai_transport_ops_t` 接口，使业务层无需修改一行代码即可在“纯开源免费链路”与“声网商业 RTC 链路”之间通过 Kconfig 切换。

```c
// components/bk_ai_agent/include/bk_ai_agent.h
typedef struct {
    const char *name;
    bk_err_t (*connect)(void *ctx, const bk_ai_session_cfg_t *cfg);
    bk_err_t (*send_audio)(void *ctx, const uint8_t *opus_frame, size_t len);
    bk_err_t (*send_image)(void *ctx, const uint8_t *jpeg_buf, size_t len, const char *prompt);
    bk_err_t (*send_control_json)(void *ctx, const char *json_str);
    bk_err_t (*disconnect)(void *ctx);
} bk_ai_transport_ops_t;

bk_err_t bk_ai_agent_init(const bk_ai_agent_config_t *cfg);
bk_err_t bk_ai_agent_register_transport(const bk_ai_transport_ops_t *ops, void *ctx);
bk_err_t bk_ai_agent_start(void);
```

---

## 五、 修正后的实施路线图（Updated Implementation Plan）

建议对原 [2026-11-12-bk-open-framework-implementation.md](file:///home/antigravity/songjw0820/bk-open-framework/docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md) 的任务序列做如下修正，使每一步都有可编译、可自动化测试的实质代码产出：

1. **Task 1：框架底座与双芯片/模拟器清单（`platforms/` + `bk_port`）**
   - 新增 `platforms/bk7258_minimal.yaml`（锁定 `release/v3.1.1.x`）与 `platforms/posix_sim.yaml`。
   - 实现 `components/bk_port/`（`posix_sim` 后端 + `bk7258_v3` / `bk7259_v4` 接口桩），跑通 PSRAM/SRAM 内存统计与跨核 IPC 环形缓冲单元测试。
2. **Task 2：CLI 工具链与 Auto-CP 代码生成器（`./tools/bk`）**
   - 实现 `doctor`、`fetch`、`set-board`、`build` 子命令。
   - 实现 `tools/bk_cp_autogen.py`：解析 `modules/*.yaml` 与 `boards/*/board.yaml`，自动生成 CP 侧引脚配置与 AP 侧板级描述表。
3. **Task 3：`bk_core` 实质化实现（异步事件总线 + KV 配置中心 + 健康监控）**
   - 实现多线程安全的 `bk_event` 队列、带 RAM 缓存与 CRC32 持久化的 `bk_kv`，以及低内存水位预警测试。
4. **Task 4：`bk_board` 与 `bk_peripheral` 实质化实现（BSP 句柄工厂 + 按键/灯效/舵机/电源引擎）**
   - 完成 `bk7259_robot` 与 `bk7258_devkit` 两套板级定义。
   - 实现多击按键状态机、优先级灯效队列、舵机余弦插补序列器与电池滤波算法，并通过 Host 模拟器测试验证全部分支。
5. **Task 5：`bk_net` 实质化实现（BLE/AP 配网协议栈 + 多 AP 漫游记忆 + 指数退避 + SNTP 门控）**
   - 联动 `bk_kv` 与 `bk_event` 实现最多 5 组 Wi-Fi 凭证按 RSSI 择优连接、断网退避重连及 `CLOUD_READY` 事件门控。
6. **Task 6：`bk_media` 与 `bk_ui` 实质化实现（双流混音器 + 零拷贝视频帧池 + 线程安全 UI 与机器人表情）**
   - 实现语音流与本地提示音的 Ducking/Preempt 混音管线、PSRAM 引用计数视频帧池及 LVGL 锁包装。
7. **Task 7：`bk_ota` 与 `bk_ai_agent` 实现（管线协同升级 + 开源 WebSocket AI 对话 + 可选 Agora 插件）**
   - 实现升级前自动挂起媒体管线的 OTA 状态机。
   - 实现内置开源 WebSocket 多模态传输层与可选 `agora_adapter` 插件接口，使 `bk7259_robot` 在无商业插件时仍能作为完整的开源 AI 机器人运行。
