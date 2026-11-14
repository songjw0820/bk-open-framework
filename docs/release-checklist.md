# BK Open Framework 发布与实机验收清单（Release & Hardware Acceptance Checklist）

> **说明**：本清单是框架进入正式版本发布（如 `v0.1.0`）的强制放行门禁（Tier 1 ~ Tier 4）。任何未填入真实日志、命令输出或实测结果的勾选框均视为**未通过（NOT VERIFIED）**。

---

## 1. 版本锁定与环境元数据记录

| 记录项 | 期望值 / 规范要求 | 实测记录值（待填写） | 状态 |
|---|---|---|---|
| 框架发布版本 Tag | `v0.1.0` | `pending` | [ ] |
| 目标参考板卡 | `bk7259_robot` (BK7259) | `pending` | [ ] |
| `bk_avdk_smp` Ref | `release/v4.0.1.11` | `pending` | [ ] |
| `bk_avdk_smp` Tag / Commit SHA | Tag `be5e89d4...` / Commit `584b9479...` | `pending` | [ ] |
| `bk_solution_ai` Ref (`robot` profile) | `release/v4.0.1.11` | `pending` | [ ] |
| `bk_solution_ai` Tag / Commit SHA | Tag `009c9518...` / Commit `a1e865e1...` | `pending` | [ ] |
| 交叉编译工具链版本 | `gcc-arm-none-eabi`（记录完整 `--version`） | `pending` | [ ] |
| 宿主机 OS / 容器镜像摘要 | Linux / Docker Image Digest | `pending` | [ ] |
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

- [ ] **Task 0 探针闭环**：运行 `./tools/spikes/bk7259_component_probe/probe.sh --sdk-dir <SDK> --solution-dir <SOL> --build` 返回 `PASS`。
- [ ] **`bk7259_minimal` 独立构建（无 `bk_solution_ai`）**：
  - [ ] 使用 `bk_avdk_smp/projects/app/cp` 作为 CP 模板，在不挂载 `bk_solution_ai` 的情况下成功编译 AP + CP 并生成 `all-app.bin`。
  - [ ] `nm` / `objdump` 确认未启用组件（如 `bk_audio_session`, `bk_video_frame_pool`, `bk_ota`, `bk_ai_agent`, `plugin_agora`）在 ELF 中零残留符号。
- [ ] **`bk7259_robot` 完整构建**：
  - [ ] 无 Agora 插件配置编译通过并生成 `all-app.bin`。
  - [ ] 启用 Agora 插件配置编译通过并生成 `all-app.bin`。
- [ ] **静态内存预算审计**：记录 AP/CP Flash、SRAM、PSRAM 占用字节数，确认无分区溢出。

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
- [ ] MIPI CSI 摄像头预览出图正常，`bk_video_frame_pool` 在 64 字节对齐与 `bk_dcache_clean_for_producer` / `bk_dcache_invalidate_for_consumer` 同步下无花屏或内存泄漏。

### 4.5 固件升级与安全校验（OTA & Rollback）
- [ ] **前置条件检查**：确认目标分区表（重规划后的 8M 分区表或 16M 分区表）中 `ota` 分区容量大于压缩固件体积，且与所选 BL2 模式（`CONFIG_SECURE_OTA_OVERWRITE` 或 `CONFIG_SECURE_OTA_XIP`）匹配。
- [ ] **媒体挂起保护**：在音视频高负载运行时发起 OTA，验证媒体管线安全挂起（或挂起失败时返回 `BK_ERR_OTA_MEDIA_BUSY` 中止升级）。
- [ ] **正常升级**：下载合法 EC-P256 签名的新版本固件，重启后成功运行新版本。
- [ ] **坏签名 / 篡改拒绝**：注入修改了 1 字节的固件或错误私钥签名固件，验证校验失败并拒绝升级，原系统保持可启动。
- [ ] **防回滚（Anti-Rollback）**：注入 `app_security_counter` 低于当前值的固件，验证被拒绝。
- [ ] **断电/断网恢复**：在下载进度 50% 时拔电或断网，重新上电后验证状态恢复与续传/重下正常，Flash 分区无损坏。
- [ ] **启动回滚验证**（仅当启用 `CONFIG_SECURE_OTA_XIP` A/B 模式时适用；若采用 `OVERWRITE` 模式需在报告中明确标注 BL2 覆盖模式不支持启动后回退旧镜像）：模拟新固件启动后未确认即崩溃，验证 BL2 自动回滚至旧槽位。

### 4.6 外设与 AI 机器人参考应用
- [ ] 按键单击、双击、长按及 LED 状态灯效在实机上响应正确。
- [ ] 小智协议（`protocol_xiaozhi`）实机连接测试服务器，完成 Hello 握手、60ms Opus 双向语音对话及 MCP 工具调用。
