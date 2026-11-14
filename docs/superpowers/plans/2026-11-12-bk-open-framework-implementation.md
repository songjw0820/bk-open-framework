# BK7258/BK7259 开源应用框架 Implementation Plan（Rev 4 · 已对齐复审结论与阻塞门禁）

> **唯一事实来源与放行门禁说明**：本实施计划与 [设计规范](../specs/2026-11-12-bk-open-framework-design.md)、[架构评审与演进规范](../../reviews/2026-11-13-architecture-review-and-espressif-comparison.md) 及 [发布验收清单](../../release-checklist.md) 保持严格同步。
> **重要约束**：在 **Task 0（使用锁定 SDK 实际跑通 `minimal` 与 `robot` 两次 `probe.sh --build` 并记录真实 `all-app.bin` 路径、大小与日志）** 完成前，**禁止进入 Task 1+ 的正式组件实现**。

**Goal:** 创建独立的 BK 开源应用框架仓库，Phase 1/2 以 `bk7259_robot`（BK7259, `release/v4.0.1.11`）为唯一验证硬件，复用 BK 官方 SDK（以及仅在 `robot` profile 下可选复用的 `bk_solution_ai`），按证据驱动的增量顺序交付具备实质状态机与安全契约的机制组件与产品组件。

**Architecture:**
- 框架仓库与 `bk_avdk_smp`、`bk_solution_ai` 外置分离，通过 manifest 锁定 `release/v4.0.1.11`（使用无歧义字段 `tag_object_sha` 与 `peeled_commit_sha`）。
- **`bk7259_minimal` 与 `bk_solution_ai` 100% 物理隔离**：`bk7259_minimal` 仅依赖 `bk_avdk_smp`，其受控 CP 模板直接取自 `bk_avdk_smp/projects/app/cp`；仅 `bk7259_robot` profile 允许引用 `bk_solution_ai/projects/beken_robot/cp`。
- 采用 **`Public API + Capability Contract + Platform Backend (bk7259_v4 / posix_sim)`** 架构，将基础机制组件（`components/*`）、暂缓目标能力组件（`bk_ota`）、产品策略组件（`product_components/*`）及专有插件（`plugins/*`）严格分离。

**Tech Stack:** C11, Make/CMake, Kconfig, Armino SMP (`release/v4.0.1.11`), Python/Shell CLI, CTest/PyTest, Docker/GitHub Actions.

**Spec:** [docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](../specs/2026-11-12-bk-open-framework-design.md)

---

### Task 0: 闭合 BK7259 AP/CP 构建集成、真实 `all-app.bin` 产物与硬件事实清单 Spike（当前前置阻塞门禁）

**Files:**
- Modify: `docs/platforms/bk7259_v4.0.1.11.md`
- Modify: `platforms/bk7259_minimal.yaml`
- Modify: `platforms/bk7259_robot.yaml`
- Modify: `tools/spikes/bk7259_component_probe/probe.sh`
- Modify: `docs/legal/bk_solution_ai-dependency-inventory.md`

**Interfaces & Gate Criteria:**
- `probe.sh --profile minimal --sdk-dir <SDK> --build` builds `bk_avdk_smp/projects/app` ONLY (without `--solution-dir`) and verifies `<SDK>/build/bk7259/app/package/all-app.bin`.
- `probe.sh --profile robot --sdk-dir <SDK> --solution-dir <SOL> --build` builds `bk_solution_ai/projects/beken_robot` and verifies `<SDK>/build/bk7259/beken_robot/package/all-app.bin`.
- Empty external component compiles on AP/CP, links into `all-app.bin`, and is excluded when its Kconfig switch is `n`.

- [x] **Step 1: Verify and record unambiguous `tag_object_sha` and `peeled_commit_sha` for `release/v4.0.1.11` in `platforms/*.yaml` and `docs/platforms/bk7259_v4.0.1.11.md`**
- [x] **Step 2: Record static SDK facts (PL310 32B cache line, 64B `bk_frame_queue` alignment, `bk_dcache_*` APIs, and `beken_robot` 8MB partition table `264K` `ota` limitation) while separating unverified peripheral DMA behaviors**
- [x] **Step 3: Fix `probe.sh` expected artifact path to `$sdk_dir/build/bk7259/$project_name/package/all-app.bin`**
- [ ] **Step 4 [BLOCKING FOR TARGET RELEASE]: In an environment with the locked SDK checkout and Armino toolchain (`cm55`/`cm52`), run `probe.sh --build` for both `minimal` and `robot`, recording real `all-app.bin` paths, byte sizes, and AP/CP build logs**
- [x] **Step 5a: Complete static license/redistribution audit for files referenced from `bk_solution_ai` (`docs/legal/bk_solution_ai-dependency-inventory.md`)**
- [ ] **Step 5b [BLOCKING FOR TARGET RELEASE]: Update `verification` status in `platforms/bk7259_*.yaml` from `not_run_toolchain_unavailable` to verified evidence after Step 4 completes**

---

### Task 1: 实现版本诊断工具 `./tools/bk` 与 Manifest 隔离校验

**Files:**
- Create: `tools/bk`
- Create: `tools/bk_manifest.py`
- Create: `tools/bk_fetch.py`
- Create: `tools/tests/test_manifest.py`
- Create: `tools/tests/test_doctor.py`

**Interfaces:**
- Validates `tag_object_sha` and `peeled_commit_sha` (rejects ambiguous `sha` fields).
- `./tools/bk doctor --profile bk7259_minimal` validates SDK checkout and confirms zero dependency on `bk_solution_ai`.
- `./tools/bk doctor --profile bk7259_robot` validates paired SDK + solution ref and `peeled_commit_sha`.

- [x] **Step 1: Write unit tests for manifest schema, `tag_object_sha`/`peeled_commit_sha` verification, and minimal profile isolation**
- [x] **Step 2: Implement `tools/bk_manifest.py`, `tools/bk_fetch.py`, and `./tools/bk`**
- [x] **Step 3: Run unit test suite (`python3 -m unittest discover -s tools/tests -v`, 12/12 PASS) (Tier 1 Gate)**
- [x] **Step 4: Commit**

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

- [x] **Step 1: Write CTest suite for `bk_port` memory stats, 64B alignment, and `bk_core_event` ordering, concurrency, and `deinit` thread join**
- [x] **Step 2: Implement `bk_common`, `bk_port` (`posix_sim` + `bk7259_v4`), and `bk_core_event`**
- [x] **Step 3a: Pass Tier 1 (`posix_sim` with ASan/UBSan: `test_event_loop` PASS)**
- [ ] **Step 3b: Pass Tier 2 (`bk7259_v4` cross-compile check with Armino toolchain)**
- [x] **Step 4: Commit**

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

- [x] **Step 1: Write tests for CRC corruption recovery, power-loss half-record rollback, sensitive credential zeroization, and `unverified` board capability rejection**
- [x] **Step 2: Implement `bk_storage`, `bk_health`, and `bk_board`**
- [x] **Step 3a: Pass Tier 1 (`test_storage_security` and `test_board_caps` PASS under ASan/UBSan)**
- [ ] **Step 3b: Pass Tier 2 (`bk7259_v4` target build gate)**
- [x] **Step 4: Commit**

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

- [x] **Step 1: Write FSM unit tests for connect, auth failure, disconnect backoff, factory reset credential wipe, and SNTP timeout handling**
- [x] **Step 2: Implement `bk_net` and M1 examples (`01_wifi_provision`, `02_lan_device`)**
- [x] **Step 3a: Verify `test_net_fsm`, `example_01_wifi_provision`, and `example_02_lan_device` pass Tier 1 (`posix_sim` without `bk_solution_ai`)**
- [ ] **Step 3b: Verify `01_wifi_provision` builds under `bk7259_minimal` with Armino toolchain and passes hardware smoke test on `bk7259_robot`**
- [x] **Step 4: Commit**

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
  4. `bk_video_frame_pool`: 64-byte aligned PSRAM buffers (`BK_FRAME_QUEUE_ALIGN_SIZE = 64U`), atomic `bk_frame_retain`/`bk_frame_release`, explicit `bk_frame_sync_for_device` / `bk_frame_sync_for_cpu` wrappers; verify actual peripheral DMA PSRAM slab requirements on `bk7259_robot` during Tier 2/4 testing before locking ownership rules.
  5. `bk_ui_core`: recursive mutex `bk_ui_lock(timeout_ms)` / `bk_ui_unlock()` around LVGL calls.

- [x] **Step 1: Write Tier 1 unit tests for button FSM, LED priority preemption, audio ringbuffer backpressure, 20ms mixer saturation math, and multi-threaded frame pool reference counting**
- [x] **Step 2: Implement each component with independent Kconfig switches and pass Tier 1 (`test_peripherals_and_media`, `example_03`, `example_04`, `example_05` PASS)**
- [ ] **Step 3: Build and verify examples `03`, `04`, `05` on `bk7259_robot`, recording verified peripheral DMA PSRAM behavior**
- [x] **Step 4: Commit**

---

### Task 6: 实现 `bk_ai_agent`（独立 `protocol_xiaozhi` MCP 主路径）、产品策略组件与 `bk7259_robot` 参考工程

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

- [x] **Step 1: Write wire-format tests for `protocol_xiaozhi` (`hello` handshake, 3×20ms->60ms Opus packetizer, MCP JSON-RPC 2.0 request/response, legacy `iot` rejection)**
- [x] **Step 2: Implement `transport_websocket`, `protocol_xiaozhi`, `product_components/*`, and optional `plugin_agora` (`test_protocol_xiaozhi` + `example_06_robot_reference` PASS)**
- [ ] **Step 3: Build `products/bk7259_robot` with Armino cross-compiler in both open-source Xiaozhi mode (no Agora) and Agora plugin mode**
- [ ] **Step 4: Execute Tier 2 ~ Tier 4 hardware gates and record complete evidence in `docs/release-checklist.md`**
- [x] **Step 5: Commit**

---

### Task 7（专项门禁解封后执行）：完成 Robot OTA 分区重算并交付暂缓目标组件 `bk_ota`

> **Deferred Gate**: `bk_ota` is excluded from default component delivery until all 5 prerequisite artifacts in Step 1 are committed and verified.

**Files:**
- Create: `boards/bk7259_robot/partitions/auto_partitions_ota.csv`
- Create: `components/bk_ota/include/bk_ota.h`
- Create: `components/bk_ota/src/bk_ota.c`
- Create: `components/bk_ota/Kconfig`
- Create: `components/bk_ota/tests/test_ota_fsm.c`

- [ ] **Step 1 [PREREQUISITE GATE]: Commit the 5 required OTA evidence artifacts for `bk7259_robot`**:
  1. Concrete re-budgeted partition CSV (replacing the `264K` `ota` limit);
  2. Measured AP and CP binary sizes;
  3. Measured compressed `ota.bin` package size;
  4. Verified BL2 security configuration;
  5. Explicit single-strategy selection (`CONFIG_SECURE_OTA_OVERWRITE` vs `CONFIG_SECURE_OTA_XIP`).
- [ ] **Step 2: Write Tier 1 tests for media quiesce failure abort (`BK_ERR_OTA_MEDIA_BUSY`), corrupted header/signature rejection, security counter anti-rollback check, and interrupted download resume**
- [ ] **Step 3: Implement `bk_ota` and run Tier 4 hardware tests on `bk7259_robot` (normal upgrade, bad signature rejection, mid-download power loss)**
- [ ] **Step 4: Promote `bk_ota` from `deferred_target_components` to `components` in `platforms/bk7259_robot.yaml` and commit**
