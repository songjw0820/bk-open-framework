# BK Open Framework 发布与实机验收清单（Release & Hardware Acceptance Checklist）

> **当前状态声明（Empty Gate Template — NOT YET VERIFIED）**：
> 本文件当前为**空门禁模板（0 项已验证）**，仅用于定义进入发布所需的验证条目，**不构成任何已通过测试的证据**。在填入真实构建产物路径、字节大小、日志摘要与实机测试记录前，所有条目均视为 **未通过（NOT VERIFIED）**。

---

## 1. 版本锁定与环境元数据记录

| 记录项 | 期望值 / 规范要求 | 实测记录值（待填写） | 状态 |
|---|---|---|---|
| 框架发布版本 Tag | `v0.1.0` | `pending` | [ ] |
| 目标参考板卡 | `bk7259_robot` (BK7259) | `pending` | [ ] |
| `bk_avdk_smp` Ref | `release/v4.0.1.11` | `pending` | [ ] |
| `bk_avdk_smp` `tag_object_sha` | `be5e89d4f6b2f6f444727cad5fd0908a8dc93499` | `pending` | [ ] |
| `bk_avdk_smp` `peeled_commit_sha` | `584b947920c414230f9909375f008e7d6a2c6c6b` | `pending` | [ ] |
| `bk_solution_ai` Ref (`robot` profile) | `release/v4.0.1.11` | `pending` | [ ] |
| `bk_solution_ai` `tag_object_sha` | `009c95182de07ae874e39e3d3c24994431bcec70` | `pending` | [ ] |
| `bk_solution_ai` `peeled_commit_sha` | `a1e865e156e8dc31dfb360219c33a6a6d908ea98` | `pending` | [ ] |
| 交叉编译工具链版本 | AP `cm55` / CP `cm52` 工具链（记录完整 `--version`） | `pending` | [ ] |
| 宿主机 OS / 容器镜像摘要 | Linux / `bekencorp/armino-idk` Image Digest | `pending` | [ ] |
| 许可证与第三方依赖审计 | [bk_solution_ai-dependency-inventory.md](legal/bk_solution_ai-dependency-inventory.md) 全部待核对项清零 | `pending` | [ ] |

---

## 2. Tier 1：Host 行为单测门禁（`posix_sim`）

- [ ] **事件总线 (`bk_core_event`)**：订阅/取消订阅、FIFO 投递顺序、队列满超时、反初始化线程回收测试通过（ASan/TSan 零报错）。
- [ ] **存储与凭证 (`bk_storage`)**：KV 读写、CRC32 损坏自动恢复、断电半写记录丢弃、`BK_STORAGE_FLAG_SENSITIVE` 加密钩子与 `bk_storage_erase_ns()` 内存/Flash 零化测试通过。
- [ ] **网络状态机 (`bk_net`)**：连接、断线指数退避重连、重复配网、SNTP 超时与 TLS 错误码传播测试通过。
- [ ] **外设状态机 (`bk_button`, `bk_led_effect`)**：消抖、单击/双击/长按、灯效优先级抢占与恢复测试通过。
- [ ] **媒体缓冲与混音算术 (`bk_audio_mixer`, `bk_video_frame_pool`)**：20ms PCM 帧混音/Ducking 防溢出、原子引用计数并发安全、帧池耗尽背压策略测试通过。
- [ ] **小智协议状态机 (`protocol_xiaozhi`)**：Hello 握手、3×20ms PCM 聚合为 60ms Opus 包、WebSocket 断线重连、MCP (`jsonrpc: 2.0`) 工具调用解析测试通过。

---

## 3. Tier 2：SDK 编译、链接与裁剪门禁（`bk7259_v4`）

- [ ] **Task 0 `minimal` 探针闭环（无 `bk_solution_ai`）**：
  - [ ] 运行 `./tools/spikes/bk7259_component_probe/probe.sh --profile minimal --sdk-dir <SDK> --build` 返回 `PASS`。
  - [ ] 记录 `<SDK>/build/bk7259/app/package/all-app.bin` 真实文件大小与 SHA256。
  - [ ] `nm` / `objdump` 确认未启用组件在 ELF 中零残留符号。
- [ ] **Task 0 `robot` 探针闭环**：
  - [ ] 运行 `./tools/spikes/bk7259_component_probe/probe.sh --profile robot --sdk-dir <SDK> --solution-dir <SOL> --build` 返回 `PASS`。
  - [ ] 记录 `<SDK>/build/bk7259/beken_robot/package/all-app.bin` 真实文件大小与 SHA256。
- [ ] **`bk7259_robot` 框架工程构建**：
  - [ ] 无 Agora 插件配置编译通过并生成 `all-app.bin`。
  - [ ] 启用 Agora 插件配置编译通过并生成 `all-app.bin`。
- [ ] **静态内存预算审计**：记录实测 AP/CP Flash、SRAM、PSRAM 占用字节数，确认无分区溢出。

---

## 4. Tier 3 & Tier 4：`bk7259_robot` 真实硬件验收门禁

### 4.1 启动与双核基础通信（Boot & AP/CP IPC）
- [ ] 烧录 `all-app.bin` 后冷启动与复位启动连续 10 次均正常进入 AP/CP 主循环，无 HardFault/Watchdog 复位。
- [ ] CP UART0 (`GPIO10/11`) 与 AP 日志输出正常，核间 IPC 通道无丢包或死锁。

### 4.2 配网、联网与断网重连（Provisioning & Networking）
- [ ] 首次无凭证开机自动进入配网模式，完成 BLE/SoftAP 配网并成功获取 IP。
- [ ] 关闭路由器电源再恢复后，设备按指数退避策略自动重连成功。
- [ ] 触发恢复出厂设置后，原 Wi-Fi 凭证被安全擦除，重启后不再自动连接旧 AP。
- [ ] 验证 SNTP 同步成功后的正常 TLS 连接，以及人为阻断 SNTP / 注入过期证书时 TLS 正确拒绝连接并返回预期错误码。

### 4.3 音频与声学前端（Audio & AEC）
- [ ] `03_audio_player` 实机播放提示音清晰无爆音（验证 PA Mute 时序）。
- [ ] 20ms PCM 采集回路正常，开启 AEC 时扬声器播报期间麦克风回采消除正常。
- [ ] 语音流播放期间触发本地提示音，Ducking / Preempt 混音行为符合预期。

### 4.4 显示与摄像头（Display & Camera）
- [ ] LCD 屏幕点亮、背光控制正常，LVGL 刷屏在多线程并发事件下无撕裂或崩溃。
- [ ] 实测确认摄像头/JPEG/LCD 外设 DMA 在 `bk7259_robot` 上所使用的 PSRAM 区域（Uncached vs Cached Slab）及 Cache Clean/Invalidate 行为，无花屏或内存泄漏。

### 4.5 外设与 AI 机器人参考应用
- [ ] 按键单击、双击、长按及 LED 状态灯效在实机上响应正确。
- [ ] 小智协议（`protocol_xiaozhi`）实机连接测试服务器，完成 Hello 握手、60ms Opus 双向语音对话及 MCP 工具调用。

---

## 5. 暂缓目标能力门禁：`bk_ota`（Deferred Target Capability Gate）

> **注意**：由于 `bk7259_robot` 现有 8MB 分区表中 `ota` 仅 `264KB` 且缺少 `ota_control`/`boot_param`，在提交并通过以下 5 项前置证据前，`bk_ota` 不进入默认组件交付：

- [ ] **前置证据 1**：提交经实测验证的 `bk7259_robot` 分区表 CSV 文件。
- [ ] **前置证据 2**：记录实测 AP 与 CP 固件镜像的准确字节大小。
- [ ] **前置证据 3**：记录实测打包出的 `ota.bin` 压缩包字节大小（确认小于新 `ota` 分区容量）。
- [ ] **前置证据 4**：提交对应的 BL2 安全配置与公钥配置证据。
- [ ] **前置证据 5**：明确选定单一升级策略（如 8MB 下选定 `CONFIG_SECURE_OTA_OVERWRITE`，并明确标注该模式不支持启动后回退旧固件）。
- [ ] **实机验证项**（上述 5 项前置证据齐备后执行）：
  - [ ] 音视频运行期间发起 OTA，验证媒体管线挂起保护（或挂起失败时返回 `BK_ERR_OTA_MEDIA_BUSY`）。
  - [ ] 合法 EC-P256 签名镜像正常升级成功。
  - [ ] 篡改 1 字节或错误签名镜像被拒绝升级。
  - [ ] `app_security_counter` 防降级拒绝验证通过。
  - [ ] 下载中途断电/断网恢复验证通过。
