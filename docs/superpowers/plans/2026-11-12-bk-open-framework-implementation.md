# BK7258/BK7259 开源应用框架 Implementation Plan（Rev 3 · 已对齐评审契约与增量路线）

> **唯一事实来源（Single Source of Truth）**：本实施计划已与 [设计规范](../specs/2026-11-12-bk-open-framework-design.md)、[架构评审与演进规范](../../reviews/2026-11-13-architecture-review-and-espressif-comparison.md) 及 [发布验收清单](../../release-checklist.md) 严格对齐。所有执行代理必须以本计划中的细粒度组件拆分、CP 模板隔离规则与四级门禁为准。

**Goal:** 创建独立的 BK 开源应用框架仓库，Phase 1/2 以 `bk7259_robot`（BK7259, `release/v4.0.1.11`）为唯一验证硬件，复用 BK 官方 SDK（以及仅在 `robot` profile 下可选复用的 `bk_solution_ai`），按证据驱动的增量顺序交付具备实质状态机与安全契约的机制组件与产品组件。

**Architecture:**
- 框架仓库与 `bk_avdk_smp`、`bk_solution_ai` 外置分离，通过 manifest 锁定 `release/v4.0.1.11`（同时记录 Tag Object SHA 与 Peeled Commit SHA）。
- **`bk7259_minimal` 与 `bk_solution_ai` 100% 物理隔离**：`bk7259_minimal` 仅依赖 `bk_avdk_smp`，其受控 CP 模板直接取自 `bk_avdk_smp/projects/app/cp`；仅 `bk7259_robot` profile 允许引用 `bk_solution_ai/projects/beken_robot/cp`。
- 采用 **`Public API + Capability Contract + Platform Backend (bk7259_v4 / posix_sim)`** 架构，将基础机制组件（`components/*`）与产品策略组件（`product_components/*`）及专有插件（`plugins/*`）严格分离。

**Tech Stack:** C11, Make/CMake, Kconfig, Armino SMP (`release/v4.0.1.11`), Python/Shell CLI, CTest/PyTest, Docker/GitHub Actions.

**Spec:** [docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](../specs/2026-11-12-bk-open-framework-design.md)

## Global Constraints

- Phase 1/2 只支持 `bk7259_robot`（BK7259）；`BK7258 (release/v3.1.1.x)` 整体归入 Phase 3，不进入当前交付门槛。
- `bk7259_minimal` 严禁直接或间接引用 `bk_solution_ai` 中的任何文件（包括 CP 配置模板）。
- 第一阶段不承诺未经验证的“全自动 CP 生成器（Auto-CP）”，仅对受控 CP 模板执行 Schema 与 diff 校验。
- 公共框架采用 Apache-2.0；Agora 等商业闭源 SDK 放入 `plugins/plugin_agora`；小智协议适配放入独立组件 `components/bk_ai_agent/protocol_xiaozhi`（声明上游协议文档为 MIT 许可证，以 MCP JSON-RPC 2.0 为唯一主路径）。
- 遵循 **Tier 1 (Host `posix_sim`) -> Tier 2 (SDK Cross-Compile `all-app.bin`) -> Tier 3 (Fault Injection) -> Tier 4 (`bk7259_robot` Real Hardware)** 四级验证门禁，未通过上一级门禁不得宣称闭合。

---

### Task 0: 闭合 BK7259 AP/CP 构建集成、分区审计与硬件事实清单 Spike（前置阻塞门禁）

**Files:**
- Modify: `docs/platforms/bk7259_v4.0.1.11.md`
- Modify: `platforms/bk7259_minimal.yaml`
- Modify: `platforms/bk7259_robot.yaml`
- Modify: `tools/spikes/bk7259_component_probe/probe.sh`
- Modify: `docs/legal/bk_solution_ai-dependency-inventory.md`

**Interfaces & Gate Criteria:**
- `bk7259_minimal` verifies AP/CP build against `bk_avdk_smp/projects/app` ONLY (without `--solution-dir`).
- `bk7259_robot` verifies AP/CP build against `bk_avdk_smp` + `bk_solution_ai/projects/beken_robot`.
- Empty external component compiles on AP/CP, links into `all-app.bin`, and is excluded when its Kconfig switch is `n`.

- [ ] **Step 1: Verify and record both Tag Object SHA and Peeled Commit SHA for `release/v4.0.1.11`**
- [ ] **Step 2: Record verified SDK facts (L2 PL310 32B cache line, 64B `bk_frame_queue` alignment, `bk_dcache_clean_for_producer`/`bk_dcache_invalidate_for_consumer`, and `beken_robot` 8MB partition table `264K` `ota` limitation) in `docs/platforms/bk7259_v4.0.1.11.md`**
- [ ] **Step 3: In an environment with the locked SDK checkout and Armino toolchain, run `probe.sh --build` for both `minimal` (`projects/app`) and `robot` (`projects/beken_robot`), verifying `all-app.bin` generation and Kconfig exclusion**
- [ ] **Step 4: Complete license/redistribution audit for any files referenced from `bk_solution_ai`**
- [ ] **Step 5: Update `verification` status in `platforms/bk7259_*.yaml` from `not_run_toolchain_unavailable` to verified evidence**

---

### Task 1: 实现版本诊断工具 `./tools/bk` 与 Manifest 隔离校验

**Files:**
- Create: `tools/bk`
- Create: `tools/bk_manifest.py`
- Create: `tools/bk_fetch.py`
- Create: `tools/tests/test_manifest.py`
- Create: `tools/tests/test_doctor.py`

**Interfaces:**
- `./tools/bk doctor --profile bk7259_minimal` validates SDK checkout SHA and confirms zero dependency on `bk_solution_ai`.
- `./tools/bk doctor --profile bk7259_robot` validates paired SDK + solution ref/SHA (supporting both tag object SHA and peeled commit SHA).
- `./tools/bk fetch --profile <profile> --root <path>` clones/checks out the locked repositories.

- [ ] **Step 1: Write unit tests for manifest schema, tag/commit SHA verification, and minimal profile isolation**
- [ ] **Step 2: Implement `tools/bk_manifest.py`, `tools/bk_fetch.py`, and `./tools/bk`**
- [ ] **Step 3: Run `python3 -m pytest tools/tests/ -v` and verify all tests pass (Tier 1 Gate)**
- [ ] **Step 4: Commit**

---

### Task 2: 实现基础契约层 `bk_common`、移植层 `bk_port` 与事件总线 `bk_core_event`

**Files:**
- Create: `components/bk_common/include/bk_err.h`
- Create: `components/bk_common/include/bk_caps.h`
- Create: `components/bk_port/include/bk_port_os.h`
- Create: `components/bk_port/include/bk_port_mem.h`
- Create: `components/bk_port/include/bk_port_cache.h`
- Create: `components/bk_port/posix_sim/`
- Create: `components/bk_port/bk7259_v4/`
- Create: `components/bk_core_event/include/bk_core_event.h`
- Create: `components/bk_core_event/src/bk_core_event.c`
- Create: `components/bk_core_event/Kconfig`
- Create: `components/bk_core_event/CMakeLists.txt`
- Create: `components/bk_core_event/tests/test_event_loop.c`

**Interfaces & Contracts:**
- `bk_port_malloc(size, caps, tag)` wraps `os_sram_malloc` / `psram_malloc` / `psram_cache_malloc` on `bk7259_v4` and enforces 64-byte alignment when `BK_MEM_CAP_DMA_ALIGN64` is requested.
- `bk_port_dcache_clean(addr, len)` / `bk_port_dcache_invalidate(addr, len)` wrap `bk_dcache_clean_for_producer` / `bk_dcache_invalidate_for_consumer` from `ap/middleware/arch/cortex-m/cache.c`.
- `bk_event_post()` deep-copies payloads (`<= 256B`); `bk_event_loop_deinit()` drains/aborts the queue, joins the worker thread, and frees all subscriber nodes.

- [ ] **Step 1: Write CTest/PyTest suite for `bk_port` memory stats, 64B alignment, and `bk_core_event` ordering, concurrency, and `deinit` thread join**
- [ ] **Step 2: Implement `bk_common`, `bk_port` (`posix_sim` + `bk7259_v4`), and `bk_core_event`**
- [ ] **Step 3: Pass Tier 1 (`posix_sim` with ASan/TSan) and Tier 2 (`bk7259_v4` compile check)**
- [ ] **Step 4: Commit**

---

### Task 3: 实现安全存储组件 `bk_storage` 与板级能力层 `bk_board`

**Files:**
- Create: `components/bk_storage/include/bk_storage.h`
- Create: `components/bk_storage/src/bk_storage.c`
- Create: `components/bk_storage/tests/test_storage_security.c`
- Create: `components/bk_board/include/bk_board.h`
- Create: `components/bk_board/src/bk_board.c`
- Create: `boards/bk7259_robot/board.yaml`
- Create: `components/bk_board/tests/test_board_caps.c`

**Interfaces & Contracts:**
- `bk_storage_set_blob(..., BK_STORAGE_FLAG_SENSITIVE)` encrypts credentials via platform key hook; `bk_storage_erase_ns()` and `bk_storage_factory_reset()` zero-out RAM cache and overwrite Flash records before sector erase.
- Append-only log record with CRC32 recovers cleanly from simulated mid-write power loss.
- `boards/bk7259_robot/board.yaml` explicitly marks each peripheral as `verified` or `unverified`; querying or initializing an `unverified` capability returns `BK_ERR_NOT_VERIFIED` or `BK_ERR_NOT_SUPPORTED`.
- CP template boundary enforced: `minimal` validates against `bk_avdk_smp/projects/app/cp`; `robot` validates against `bk_solution_ai/projects/beken_robot/cp`.

- [ ] **Step 1: Write tests for CRC corruption recovery, power-loss half-record rollback, sensitive credential zeroization, and `unverified` board capability rejection**
- [ ] **Step 2: Implement `bk_storage` and `bk_board`**
- [ ] **Step 3: Pass Tier 1 and Tier 2 gates**
- [ ] **Step 4: Commit**

---

### Task 4: 实现 `bk_net`（配网状态机、多 AP 凭证、退避重连与 SNTP/TLS 契约）及 M1 示例

**Files:**
- Create: `components/bk_net/include/bk_net.h`
- Create: `components/bk_net/src/bk_net.c`
- Create: `components/bk_net/src/bk_provision.c`
- Create: `components/bk_net/src/bk_lan.c`
- Create: `components/bk_net/tests/test_net_fsm.c`
- Create: `examples/01_wifi_provision/`
- Create: `examples/02_lan_device/`

**Interfaces & Contracts:**
- Stores up to 5 Wi-Fi credentials in `bk_storage` with `BK_STORAGE_FLAG_SENSITIVE`.
- Exponential backoff reconnect (`1s..30s` with jitter).
- Distinguishes `BK_NET_STATE_IP_READY` from `BK_NET_STATE_TIME_SYNCED`; if SNTP fails, surfaces `BK_ERR_NET_TIME_UNSYNCED` and never disables TLS certificate chain or SNI verification in production builds.

- [ ] **Step 1: Write FSM unit tests for connect, auth failure, disconnect backoff, factory reset credential wipe, and SNTP timeout handling**
- [ ] **Step 2: Implement `bk_net` and M1 examples (`01_wifi_provision`, `02_lan_device`)**
- [ ] **Step 3: Verify `01_wifi_provision` builds under `bk7259_minimal` without `bk_solution_ai` and passes hardware smoke test on `bk7259_robot`**
- [ ] **Step 4: Commit**

---

### Task 5: 增量实现外设状态机（`bk_button`, `bk_led_effect`）与单一媒体组件（`bk_audio_session`, `bk_video_frame_pool`, `bk_ui_core`）

**Files:**
- Create: `components/bk_button/`
- Create: `components/bk_led_effect/`
- Create: `components/bk_audio_session/`
- Create: `components/bk_audio_mixer/`
- Create: `components/bk_video_frame_pool/`
- Create: `components/bk_ui_core/`
- Create: `examples/03_audio_player/`
- Create: `examples/04_camera_stream/`
- Create: `examples/05_peripheral_control/`

**Interfaces & Real-Time Contracts:**
- Deliver and verify one component at a time (do not bundle with AI or OTA):
  1. `bk_button` (debounce + click/double-click/long-press FSM) and `bk_led_effect` (priority queue).
  2. `bk_audio_session`: fixed 20ms PCM frame (`16kHz/16-bit/Mono` = 640B), synchronous copy into downlink ringbuffer with explicit timeout/backpressure (`BK_ERR_AUDIO_RINGBUF_FULL`), hardware AEC capability check.
  3. `bk_audio_mixer`: optional 20ms PCM Ducking/Preempt mixer.
  4. `bk_video_frame_pool`: 64-byte aligned PSRAM buffers (`BK_FRAME_QUEUE_ALIGN_SIZE = 64U`), atomic `bk_frame_retain`/`bk_frame_release`, explicit `bk_frame_sync_for_device` (`bk_dcache_clean_for_producer`) and `bk_frame_sync_for_cpu` (`bk_dcache_invalidate_for_consumer`), `BK_ERR_BUSY` on `destroy` if `ref_cnt > 0`.
  5. `bk_ui_core`: recursive mutex `bk_ui_lock(timeout_ms)` / `bk_ui_unlock()` around LVGL calls.

- [ ] **Step 1: Write Tier 1 unit tests for button FSM, LED priority preemption, audio ringbuffer backpressure, 20ms mixer saturation math, and multi-threaded frame pool reference counting**
- [ ] **Step 2: Implement each component with independent Kconfig switches and verify disabled components add zero bytes to `bk7259_minimal`**
- [ ] **Step 3: Build and verify examples `03`, `04`, `05` on `bk7259_robot`**
- [ ] **Step 4: Commit**

---

### Task 6: 分区重规划验证与实现 `bk_ota`（前置媒体挂起与 BL2 安全校验集成）

**Files:**
- Create: `components/bk_ota/include/bk_ota.h`
- Create: `components/bk_ota/src/bk_ota.c`
- Create: `components/bk_ota/Kconfig`
- Create: `components/bk_ota/tests/test_ota_fsm.c`

**Interfaces & Platform Evidence Prerequisites:**
- **Prerequisite Gate**: Re-budget `bk7259_robot` partition table (resolving the default `264K` `ota` partition limit in `bk_solution_ai/projects/beken_robot/partitions/bk7259/auto_partitions.csv`) and select the verified SDK OTA backend (`CONFIG_SECURE_OTA_OVERWRITE` or `CONFIG_SECURE_OTA_XIP` with EC-P256 BL2 verification).
- **Pre-OTA Media Quiesce**: `bk_ota_start()` requests `bk_audio_session` and `bk_video_frame_pool` to suspend within `quiesce_timeout_ms`; if media fails to quiesce, abort immediately with `BK_ERR_OTA_MEDIA_BUSY`.
- **Integrity & Resume**: Verify `"BK723658"` global header and CRC32 / EC-P256 signature; checkpoint verified offset only at coarse intervals (e.g., 256KB boundaries) to avoid Flash wear.

- [ ] **Step 1: Write Tier 1 tests for media quiesce failure abort (`BK_ERR_OTA_MEDIA_BUSY`), corrupted header/signature rejection, security counter anti-rollback check, and interrupted download resume**
- [ ] **Step 2: Verify partition table layout and BL2 configuration on `bk7259_robot`**
- [ ] **Step 3: Implement `bk_ota` and run Tier 4 hardware tests (normal upgrade, bad signature rejection, mid-download power loss)**
- [ ] **Step 4: Commit**

---

### Task 7: 实现 `bk_ai_agent`（独立 `protocol_xiaozhi` MCP 主路径）、产品策略组件与 `bk7259_robot` 参考工程

**Files:**
- Create: `components/bk_ai_agent/transport_websocket/`
- Create: `components/bk_ai_agent/protocol_xiaozhi/`
- Create: `product_components/bk_motion/`
- Create: `product_components/bk_robot_face/`
- Create: `product_components/bk_battery_policy/`
- Create: `plugins/plugin_agora/`
- Create: `products/bk7259_robot/`
- Create: `examples/06_robot_reference/`
- Modify: `docs/release-checklist.md`

**Interfaces & Protocol Contracts:**
- `transport_websocket`: RFC 6455 WSS client (Text/Binary frames, Ping/Pong, reconnect).
- `protocol_xiaozhi`:
  - Clean-room implementation of the Xiaozhi WebSocket protocol (upstream protocol spec is **MIT licensed**; record attribution in `components/bk_ai_agent/protocol_xiaozhi/NOTICE`).
  - **20ms -> 60ms Audio Packetization**: Aggregates 3 consecutive 20ms PCM frames (`3 * 320 = 960 samples`) from `bk_audio_session` into a single 60ms Opus frame when `frame_duration: 60` is negotiated in the `hello` handshake; splits incoming 60ms Opus frames back into 3×20ms PCM frames for playback.
  - **MCP Primary Path**: Implements MCP (`{"type": "mcp", "payload": {"jsonrpc": "2.0", ...}}`) as the sole primary device tool-calling protocol; legacy `iot` messages are excluded by default (`CONFIG_BK_XIAOZHI_LEGACY_IOT_COMPAT=n`).
- `plugins/plugin_agora`: Optional proprietary RTC/RTM plugin; `products/bk7259_robot` builds and runs full voice/vision/motion interaction in open-source `protocol_xiaozhi` mode without Agora, and optionally switches to Agora when the plugin is enabled.

- [ ] **Step 1: Write wire-format tests for `protocol_xiaozhi` (`hello` handshake, 3×20ms->60ms Opus packetizer, MCP JSON-RPC 2.0 request/response, disconnect recovery) against a local Mock Server**
- [ ] **Step 2: Implement `transport_websocket`, `protocol_xiaozhi`, `product_components/*`, and optional `plugin_agora`**
- [ ] **Step 3: Build `products/bk7259_robot` in both open-source Xiaozhi mode (no Agora) and Agora plugin mode**
- [ ] **Step 4: Execute all Tier 1 ~ Tier 4 gates and record complete evidence in `docs/release-checklist.md`**
- [ ] **Step 5: Tag `v0.1.0`**
