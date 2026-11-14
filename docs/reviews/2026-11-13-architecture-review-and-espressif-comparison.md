# BK Open Framework 架构深度评审与演进规范（Rev 3 方案修订，待验证项已明确）

- **文档路径**：`docs/reviews/2026-11-13-architecture-review-and-espressif-comparison.md`
- **版本状态**：`Rev 3.0 方案修订，待验证项已明确`（根据 Codex 对 `1623bed` 的复审意见，明确区分“方案/文档已修订”与“代码/构建/实机待验证”，并补齐 `bk_avdk_smp` 静态源码证据、解决 `minimal` CP 模板依赖冲突、同步更新全仓 Spec/Plan 与 `docs/release-checklist.md`）
- **唯一事实来源同步文件**：
  - 设计规范：[docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](../superpowers/specs/2026-11-12-bk-open-framework-design.md)
  - 实施计划：[docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md)
  - 平台证据：[docs/platforms/bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)
  - 发布验收清单：[docs/release-checklist.md](../release-checklist.md)
  - 平台清单：[platforms/bk7259_minimal.yaml](../../platforms/bk7259_minimal.yaml)、[platforms/bk7259_robot.yaml](../../platforms/bk7259_robot.yaml)

---

## 零、 Codex 复审意见修订与验证状态跟踪表（Rev 2 -> Rev 3）

| 复审编号 | Codex 复审指出的未闭合问题 | Rev 3 方案修订内容 | 当前验证状态 |
| :--- | :--- | :--- | :--- |
| **P0-1** | **文档修订不等于问题已验证**：无代码、无 `all-app.bin` 构建、无实机证据，不能宣称“已闭合”。 | 标题与全文状态修正为 **“Rev 3 方案修订，待验证项已明确”**；所有未跑通构建与硬件的条目显式标注为 `待验证（Pending Verification）`。 | **方案已修订；代码与构建待验证** |
| **P0-2** | **受控 CP 模板违反 minimal profile 解耦**：若直接复用 `bk_solution_ai` 的 CP 模板，会导致 `bk7259_minimal` 间接依赖 solution 且引入未审计代码。 | **采用分源隔离方案**：经核查 `bk_avdk_smp` (`release/v4.0.1.11`) 源码树，SDK 自身已包含 `projects/app/cp/`（含 `CMakeLists.txt`, `cp_main.c`, `config/bk7259/defconfig`, `usr_gpio_cfg.h`）。因此规定：<br>1. **`bk7259_minimal` 的 CP 模板严格取自 `bk_avdk_smp/projects/app/cp`（零 solution 依赖）**；<br>2. **`bk7259_robot` 的 CP 模板取自 `bk_solution_ai/projects/beken_robot/cp`（仅 robot profile 使用）**。 | **架构冲突已消除；已同步写入 `platforms/*.yaml`，待构建验证** |
| **P0-3** | **OTA 安全契约缺少 BK7259 平台证据**：签名算法、信任根、防回滚、双分区容量与断电磨损未从实际 SDK/分区表核实。 | 已通过 GitHub API 核查 `bk_avdk_smp` 与 `bk_solution_ai` (`release/v4.0.1.11`) 源码并记录到 [bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)：<br>1. **签名算法**：BL2 唯一使用 **ECDSA P-256 (`ec256`)**（见 `security.csv` 与 `ap/components/ota/Kconfig`）；<br>2. **两种互斥模式**：`CONFIG_SECURE_OTA_OVERWRITE`（单 `ota` 暂存区解压覆盖 `primary_all`，无启动后回退旧镜像能力）与 `CONFIG_SECURE_OTA_XIP`（A/B 双槽位 + `boot_param` `TRIAL` 回滚）；<br>3. **关键分区阻塞事实**：`bk_solution_ai/projects/beken_robot/partitions/bk7259/auto_partitions.csv`（8MB Flash）中 `primary_ap_app` 占 `6248K`，导致 **`ota` 分区仅剩 `264K`** 且无 `ota_control`/`boot_param` 分区！<br>4. 因此将安全 OTA 与回滚明确列为**需先完成分区表重算与 BL2 验证的目标能力**，并在 `ota_secure_overwrite` 下不承诺启动后回退旧固件。 | **静态代码与分区表证据已核实；实机升级与分区重算待验证** |
| **P1-1** | **DMA/Cache 规格未经平台验证**：缺少 BK7259 Cache line 大小、实际 Cache API 与 PSRAM 对齐的 SDK 源码来源。 | 已核查 `bk_avdk_smp` (`release/v4.0.1.11`) 源码并补齐确切来源：<br>- **L2 Cache Line**：`ap/middleware/arch/cortex-m/l2_cache.c` 定义 `L2C_PL310_BASE = 0xA0000000U`, `L2C_CACHE_LINE_SIZE = 32U`（32B）；<br>- **帧缓冲对齐宏**：`ap/components/bk_frame_buffer/include/bk_frame_queue.h` 定义 `BK_FRAME_QUEUE_ALIGN_SIZE = 64U`（64B 对齐）；<br>- **实际 Cache API**：`ap/middleware/arch/cortex-m/cache.c` 提供 `bk_dcache_addr_is_cacheable()`, `bk_dcache_clean_for_producer()`, `bk_dcache_invalidate_for_consumer()`；<br>- **PSRAM 堆 API**：`ap/include/os/mem.h` 提供 `psram_malloc()` 与 `psram_cache_malloc()`。 | **SDK 源码依据已核实；外设 DMA 实机行为待验证** |
| **P1-2** | **音频内部格式（20ms）与小智协议参数（60ms）不一致**：需明确帧聚合与拆分关系。 | 明确规定三级转换链：<br>- **采集/AEC 层 (`bk_audio_session`)**：固定 `20 ms` PCM 帧（16kHz/16-bit/Mono = 320 samples = 640B）；<br>- **协议打包层 (`protocol_xiaozhi`)**：当握手协商 `frame_duration: 60` 时，将 **3 个连续 20 ms PCM 帧（960 samples）聚合编码为 1 个 60 ms Opus 包**发送；下行收到 60 ms Opus 包解码后拆分为 3 个 20 ms PCM 帧送入播放环。 | **方案已修订；待单测与实机验证** |
| **P1-3** | **小智协议仍保留过时的 `iot`**：上游已将 `iot` 标为 deprecated，MCP 是主路径。 | 修正为：**Phase 2 只实现 MCP（[小智 MCP 协议](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-protocol.md)，JSON-RPC 2.0）作为唯一主路径**；传统 `iot` 仅作为默认关闭的旧服务器兼容开关（`CONFIG_BK_XIAOZHI_LEGACY_IOT_COMPAT=n`）。 | **方案已修订** |
| **P1-4** | **`docs/release-checklist.md` 尚不存在**：缺失 Tier 4 实机验收表文件。 | 已在仓库中创建 [docs/release-checklist.md](../release-checklist.md)，覆盖版本/SHA、Tier 1~4 门禁、启动、配网、音频/AEC、LCD/Camera、OTA 正常/坏签名/断电/回滚及小智 MCP 验收项。 | **文件已创建；待实机执行填写** |
| **P1-5** | **评审文档与实施计划不一致**：原 Spec 与 Implementation Plan 仍保留旧组件名与“thin adapters”描述。 | 已同步更新 [设计规范](../superpowers/specs/2026-11-12-bk-open-framework-design.md)、[实施计划](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md) 与 [platforms/*.yaml](../../platforms/bk7259_minimal.yaml)，统一组件命名、CP 模板来源与任务定义。 | **全仓文档已统一** |

---

## 一、 现状诊断与阶段范围收敛（Current State & Scope Alignment）

### 1.1 仓库与上游 SDK 静态核查事实
1. **本仓库状态**：仍处于 Task 0 规划与静态探针阶段，`components/`、`boards/`、`examples/`、`products/` 尚未创建；AP/CP 编译与 `all-app.bin` 打包状态为 `not_run_toolchain_unavailable`。
2. **上游标签与 Commit SHA 核查事实**（详见 [bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)）：
   - `bekencorp/bk_avdk_smp` 的 `release/v4.0.1.11` 标签对象 SHA 为 `be5e89d4f6b2f6f444727cad5fd0908a8dc93499`，解引用后的 Commit SHA 为 `584b947920c414230f9909375f008e7d6a2c6c6b`。
   - `bekencorp/bk_solution_ai` 的 `release/v4.0.1.11` 标签对象 SHA 为 `009c95182de07ae874e39e3d3c24994431bcec70`，解引用后的 Commit SHA 为 `a1e865e156e8dc31dfb360219c33a6a6d908ea98`。
3. **阶段范围收敛**：
   - **Phase 1 / Phase 2**：唯一目标硬件为 **`bk7259_robot`（BK7259, `release/v4.0.1.11`）**，仅实现 `bk7259_v4` 与 `posix_sim` 后端。
   - **Phase 3（远期扩展）**：`BK7258 (release/v3.1.1.x)` 及 `bk7258_v3` 后端归入 Phase 3，不进入当前交付门槛，不阻塞 BK7259 `v0.1.0` 发布。

---

## 二、 架构关键设计修订（全部待代码与构建验证）

### 2.1 受控 CP 模板分源隔离（解决 P0-2 Minimal 与 Solution 依赖冲突）

为严格保证 `bk7259_minimal` 不依赖 `bk_solution_ai` 且不引入未经审计的 solution 代码，两个 Profile 的 CP 模板实行**分源隔离**：

| Profile | 依赖仓库 | 受控 CP 模板来源路径 | 适用范围与约束 |
| :--- | :--- | :--- | :--- |
| **`bk7259_minimal`** | 仅 `bk_avdk_smp` | `bk_avdk_smp/projects/app/cp/`<br>（含 `CMakeLists.txt`, `cp_main.c`, `vnd_cal.c`, `config/bk7259/defconfig`, `usr_gpio_cfg.h`） | **100% 来自官方 SDK**，不克隆、不引用 `bk_solution_ai` 的任何文件；用于验证最小框架、事件、存储与基础联网示例。 |
| **`bk7259_robot`** | `bk_avdk_smp` + `bk_solution_ai` | `bk_solution_ai/projects/beken_robot/cp/`<br>（含机器人板级 `config/bk7259/usr_gpio_cfg.h` 等配置） | **仅在 `robot` profile 下启用**；使用前必须完成 [bk_solution_ai-dependency-inventory.md](../legal/bk_solution_ai-dependency-inventory.md) 中的逐文件许可证核对。 |

### 2.2 DMA / Cache / PSRAM 平台事实与契约（解决 P1-1）

基于 `bk_avdk_smp` (`release/v4.0.1.11`) 源码核实的硬件与 SDK 接口事实（实机 DMA 行为待 Tier 2/Tier 4 验证）：

1. **Cache 硬件与对齐规格来源**：
   - **L2 Cache Line 大小**：`ap/middleware/arch/cortex-m/l2_cache.c` 中定义 PL310 控制器 `L2C_CACHE_LINE_SIZE = 32U`（32 字节）。
   - **SDK 帧缓冲与 Slab 对齐宏**：`ap/components/bk_frame_buffer/include/bk_frame_queue.h` 定义 `BK_FRAME_QUEUE_ALIGN_SIZE = 64U`；`bk_mem_slab.h` 定义 `ALIGN_BYTES = 64`。因此本框架 `bk_video_frame_pool` 统一采用 **64 字节对齐（`BK_FRAME_QUEUE_ALIGN_SIZE`）**，同时满足 32B Cache Line 与 64B DMA 突发对齐要求。
2. **SDK 实际 Cache 一致性 API 来源**：
   - 位于 `ap/middleware/arch/cortex-m/cache.c`：
     - `bk_dcache_addr_is_cacheable(const void *addr, size_t len)`：检查地址是否落在 `CONFIG_PSRAM_MPU_REGION_0..3` 的 Cacheable MPU 区域内。
     - `bk_dcache_clean_for_producer(void *addr, size_t len)`：当地址可缓存时，调用 `arch_dcache_flush_range(addr, len)`（Clean L1 + PL310 L2）并执行 `__DSB()`。
     - `bk_dcache_invalidate_for_consumer(void *addr, size_t len)`：当地址可缓存时，调用 `arch_dcache_invd_range(addr, len)`（Invalidate L1 + PL310 L2）。
3. **PSRAM 内存池与 CPU/DMA Ownership 切换流程**：
   - SDK 在 `projects/beken_robot/partitions/bk7259/ram_regions.csv` 与 `ap/include/os/mem.h` 中区分了 Uncached PSRAM Slab（`PSRAM_MEM_SLAB_UNCODED`, `0x60000000`）、Cached PSRAM Slab（`PSRAM_MEM_SLAB_CODED`, `0x64000000`）、`psram_malloc()` 与 `psram_cache_malloc()`。
   - **Ownership 切换规则**：
     - **DMA 写入 -> CPU 读取**：DMA 写入前若缓冲区可能存在脏 Cache 行，先执行 `bk_dcache_invalidate_for_consumer(buf, alloc_size)`；DMA 完成中断/回调触发后，CPU 读取前再次确认 Invalidate（若分配自 Uncached 区域，`bk_dcache_addr_is_cacheable()` 自动返回 `false` 并短路为空操作）。
     - **CPU 写入 -> DMA 读取（如 LCD 刷屏/硬件编码）**：CPU 写入完成后，移交 DMA 前必须调用 `bk_dcache_clean_for_producer(buf, alloc_size)`，确保数据刷回物理 PSRAM。

### 2.3 OTA 与 Bootloader 平台静态证据 vs 目标能力边界（解决 P0-3）

经核查 `bk_avdk_smp` 与 `bk_solution_ai` (`release/v4.0.1.11`) 源码，必须将**已核实的 SDK 现状**与**待验证的目标能力**严格区分：

#### (1) 已从 SDK 源码核实的事实
1. **签名算法唯一性**：BL2 安全启动与安全 OTA 仅支持 **ECDSA P-256 (`ec256`)**（见 `projects/secureboot_overwrite/partitions/bk7259/security.csv` 中 `root_key_type,ec256` 及 `ap/components/ota/Kconfig`）。
2. **SDK 提供的两种互斥安全 OTA 架构**（见 `ap/components/ota/Kconfig`）：
   - **模式 A：`CONFIG_SECURE_OTA_OVERWRITE`**（`ap/components/ota/ota_secure_overwrite/ota_secure_overwrite.c`）
     - 机制：将压缩且带 BL2 EC-P256 签名的镜像（带 32B `"BK723658"` 魔数头与 `CheckSumUtils` 非取反 CRC32）写入单一 `ota` 分区，下载校验完成后在 `ota_control` 分区末尾写入 12 字节确认记录 `struct ota_confirm_rec { magic = 0x4F544143 ('OTAC'), confirm = OVERWRITE_CONFIRM (0xa16d8fb0), crc }`。
     - 重启后行为：BL2 读取 `ota_control`，校验 EC-P256 签名与 `app_security_counter`（`ota.csv`），将 `ota` 分区解压覆盖到 `primary_all`。
     - **能力边界**：**Overwrite 模式在 BL2 覆盖 `primary_all` 后，旧固件已被覆盖，因此不支持新固件启动进入应用层后的“未确认自动回滚旧版本”**（仅在解压/验签阶段防止损坏镜像启动）。
   - **模式 B：`CONFIG_SECURE_OTA_XIP`**（`ap/components/ota/ota_secure_xip/ota_secure_xip.c` & `ota_boot_param.c`）
     - 机制：要求 Flash 具备 `primary_all` 与 `secondary_all` A/B 双 XIP 槽位及 `boot_param` 分区；新固件写入非活动槽位后在 `boot_param` 写入 `TRIAL` 记录，支持 BL2 EC-P256 验签与未确认回滚。
3. **`bk7259_robot` 现有 8MB 分区表的硬约束（阻塞点）**：
   - 在 `bk_solution_ai/projects/beken_robot/partitions/bk7259/auto_partitions.csv`（`FLASH_CAPACITY=8M`）中：
     - `primary_cp_app = 1440K`
     - `primary_ap_app = 6248K`（注释注明实测占用约 `3700K`）
     - **`ota = 264K`**，且未配置 `ota_control`、`boot_param` 或 `secondary_all` 分区。
   - **结论**：8MB Flash 无法容纳 `3.7MB + 1.4MB` 的双份 A/B 镜像（排除 `CONFIG_SECURE_OTA_XIP`）；即便采用 `CONFIG_SECURE_OTA_OVERWRITE`，现有的 `264K` `ota` 分区也无法放下一份完整的 AP+CP 压缩包。

#### (2) Phase 2 `bk_ota` 的目标能力与前置门禁（未验证前不宣称已具备）
- **前置门禁 1（分区表重算验证）**：在 `bk7259_robot` 8MB Flash 上，必须先验证将 `primary_ap_app` 从 `6248K` 缩减至约 `4200K`，腾出约 `2200K` 给 `ota` 暂存区并增设 `ota_control` 分区，从而适配 **`CONFIG_SECURE_OTA_OVERWRITE`（单一 EC-P256 签名压缩覆盖模式）**。
- **前置门禁 2（Flash 磨损控制）**：断点续传不得高频写入 Flash KV；仅允许在 RAM 中维护实时偏移，或仅在 256KB 扇区整数倍边界/网络异常断开时记录一次检查点。
- **前置门禁 3（媒体管线协同）**：OTA 开始前同步请求挂起 `bk_audio_session` 与 `bk_video_frame_pool`；若在 `quiesce_timeout_ms` 内挂起失败，立即中止并返回 `BK_ERR_OTA_MEDIA_BUSY`。

### 2.4 音频内部帧长（20ms）与小智协议帧长（60ms）适配规范（解决 P1-2 & P1-3）

1. **20ms PCM 与 60ms Opus 帧长转换链（解决 P1-2）**：
   - **采集与声学前端层 (`bk_audio_session`)**：内部固定以 **`20 ms` PCM 帧**（16kHz / 16-bit Signed LE / Mono = 320 samples = 640 bytes）为单位运转，匹配硬件/软件 AEC 算法的帧长约束。
   - **小智协议打包层 (`protocol_xiaozhi`)**：
     - 上行（Uplink）：当与小智服务端在 `hello` 握手中协商 `frame_duration: 60` 时，协议适配层内置一个 3 帧聚合缓冲器（Packetizer），将来自 `bk_audio_session` 的 **3 个连续 20 ms PCM 帧（共 960 samples）聚合编码为 1 个 60 ms Opus 帧**（或按 Opus 多帧打包规范封装），再通过 1 个 WebSocket Binary Frame 发送。
     - 下行（Downlink）：收到服务端的 60 ms Opus 二进制帧后，解码产出 960 samples（60 ms PCM），按 **3 × 20 ms（每次 640 bytes）** 依次推入 `bk_audio_session_push_downlink()`。
2. **以 MCP (JSON-RPC 2.0) 为唯一主路径（解决 P1-3）**：
   - 根据 [小智 WebSocket 协议文档](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md) 与 [小智 MCP 协议文档](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-protocol.md)，传统 `iot` 描述符与命令机制已废弃（deprecated）。
   - **Phase 2 强制规范**：`protocol_xiaozhi` **仅实现 MCP（`{"type": "mcp", "payload": {"jsonrpc": "2.0", ...}}`）作为标准端侧工具注册与调用路径**；传统 `iot` 仅保留默认关闭的编译宏 `CONFIG_BK_XIAOZHI_LEGACY_IOT_COMPAT=n`，不作为主协议设计与测试目标。

---

## 三、 放行进入代码实现前的检查清单（Pre-Implementation Readiness Gate）

根据 Codex 复审结论，在本仓库完成以下前置验证前，本文档作为 **Rev 3 架构与契约草案**，指导后续按序执行：

1. [x] **消除 `bk_solution_ai` CP 模板与 `minimal` profile 的依赖冲突**：已明确 `bk7259_minimal` 绑定 `bk_avdk_smp/projects/app/cp`，同步写入 `platforms/bk7259_minimal.yaml`。
2. [x] **统一全仓唯一事实来源**：已同步更新 [设计规范](../superpowers/specs/2026-11-12-bk-open-framework-design.md)、[实施计划](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md) 与 [平台证据](../platforms/bk7259_v4.0.1.11.md)。
3. [x] **创建发布与实机验收清单**：已创建 [docs/release-checklist.md](../release-checklist.md)。
4. [x] **核实 BK7259 SDK 的 Cache/DMA 与 OTA/分区表源码证据**：已从 `bk_avdk_smp` (`release/v4.0.1.11`) 提取确切源码路径与宏定义并记入平台证据文档。
5. [ ] **【待工具链环境执行】完成 Task 0 编译闭环**：使用锁定版 SDK 完成 AP/CP 空组件编译与 `all-app.bin` 打包验证，将 `not_run_toolchain_unavailable` 更新为实测通过记录。
