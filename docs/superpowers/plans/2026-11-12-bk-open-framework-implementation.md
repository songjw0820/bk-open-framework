# BK7258/BK7259 开源应用框架 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建独立的 BK 开源应用框架仓库，第一阶段以 `bk7259_robot` 为唯一验证板卡，复用 BK 官方 SDK 和 solution，提供可复用的联网、音视频、UI、OTA、外设和机器人应用组件。

**Architecture:** 框架仓库与 `bk_avdk_smp`、`bk_solution_ai` 外置分离，通过 manifest 锁定 `release/v4.0.1.11` 和 commit SHA。SDK 是必需依赖，`bk_solution_ai` 只在 `bk7259_robot` profile 中作为可选参考依赖。公共组件位于框架仓库，芯片和板卡差异位于 platform/board 适配层，专有 Agora/AI 云端能力位于可选插件。

**Tech Stack:** C, Make/CMake, Kconfig, Armino SMP, shell/Python tooling, Docker, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-12-bk-open-framework-design.md`

## Global Constraints

- 第一阶段只支持 `bk7259_robot`。
- `bk_avdk_smp` 必须锁定 `release/v4.0.1.11` 并记录 commit SHA；只有 `bk7259_robot` profile 锁定匹配的 `bk_solution_ai` ref/SHA。
- 不复制 Armino SDK 或官方 solution 的完整源码到框架仓库。
- 通用最小联网 profile 不得强依赖 `bk_solution_ai`；只有机器人 profile 才锁定 solution。
- 公共框架采用 Apache-2.0；Agora、AI 云端 SDK 等专有依赖不得进入核心依赖。
- 保留 Armino 原生 Make/Kconfig，新增轻量构建和诊断包装命令。
- 第一阶段优先支持 Linux/macOS + Docker，中文文档优先。

## Review Focus

- SDK/solution tag 不一致时，诊断工具必须在构建前失败。
- 缺少 `bk7259_robot` 板级能力时，组件必须给出配置错误而不是运行期崩溃。
- 未启用可选媒体/OTA/插件时，最小联网示例不得引入对应依赖。
- 网络断开、重复配网和重启场景必须保持可恢复状态。
- 组件 API 不得把 BK7259 芯片宏泄漏到产品业务代码。

---

### Task 0: 完成 BK7259 AP/CP 构建集成与硬件事实清单 Spike

**Files:**
- Create: `bk-open-framework/docs/platforms/bk7259_v4.0.1.11.md`
- Create: `bk-open-framework/platforms/bk7259_minimal.yaml`
- Create: `bk-open-framework/platforms/bk7259_robot.yaml`
- Create: `bk-open-framework/tools/spikes/bk7259_component_probe/`
- Create: `bk-open-framework/docs/legal/bk_solution_ai-dependency-inventory.md`

**Interfaces:**
- `bk7259_minimal` requires only Armino SMP SDK.
- `bk7259_robot` additionally pins matching `bk_solution_ai` ref/SHA.
- The probe proves where external components are injected into AP/CP Make/CMake/Kconfig builds and records exact build entry points.

- [ ] **Step 1: Record real SDK and solution SHAs for `release/v4.0.1.11`**
- [ ] **Step 2: Inspect `bk7259_robot` board configuration and hardware documents; record verified and unverified capabilities**
- [ ] **Step 3: Build an empty external component in both required AP/CP integration points; pass only when both side artifacts compile and the final `all-app.bin` link succeeds**
- [ ] **Step 4: Inventory licenses, resources, models and proprietary dependencies copied or referenced by `bk_solution_ai`**
- [ ] **Step 5: Commit the spike evidence and unblock later component tasks**

### Task 1: 创建独立框架仓库骨架与版本清单

**Files:**
- Create: `bk-open-framework/LICENSE`
- Create: `bk-open-framework/README.md`
- Create: `bk-open-framework/README_CN.md`
- Create: `bk-open-framework/components/.gitkeep`
- Create: `bk-open-framework/boards/.gitkeep`
- Create: `bk-open-framework/examples/.gitkeep`
- Create: `bk-open-framework/products/.gitkeep`
- Create: `bk-open-framework/tools/.gitkeep`

**Interfaces:**
- Produces manifest fields `chip`, `board`, `profile`, `sdk.repository`, `sdk.ref`, `sdk.sha`, and optional `solution.repository`, `solution.ref`, `solution.sha`.

- [ ] **Step 1: Write manifest validation tests**

Add shell/Python tests covering valid `bk7259_minimal` and `bk7259_robot` manifests, missing SHA, mismatched SDK/solution refs, and unsupported chip/board.

- [ ] **Step 2: Run tests and verify the invalid cases fail**

Run: `python3 -m pytest tools/tests/test_manifest.py -v`
Expected: invalid manifests fail with explicit validation errors.

- [ ] **Step 3: Implement manifest schema and repository skeleton**

Create Apache-2.0 license, bilingual README, directory layout, and the `bk7259_robot` manifest pinned to `release/v4.0.1.11`.

- [ ] **Step 4: Run manifest tests**

Run: `python3 -m pytest tools/tests/test_manifest.py -v`
Expected: all manifest validation tests pass.

- [ ] **Step 5: Commit**

```bash
git add .
git commit -m "chore: scaffold bk open framework"
```

### Task 2: 实现 SDK/solution 获取与版本诊断工具

**Files:**
- Create: `bk-open-framework/tools/bk`
- Create: `bk-open-framework/tools/bk_manifest.py`
- Create: `bk-open-framework/tools/bk_fetch.py`
- Create: `bk-open-framework/tools/tests/test_doctor.py`
- Modify: `bk-open-framework/README_CN.md`

**Interfaces:**
- `./tools/bk doctor --profile bk7259_robot` validates manifest, refs, SHA and the optional solution checkout.
- `./tools/bk fetch --profile bk7259_minimal --root <path>` clones or updates the SDK only; robot profile also fetches solution.
- `./tools/bk env --profile bk7259_robot` prints `SDK_DIR`, optional solution path and selected board.

- [ ] **Step 1: Write failing doctor tests**

Test absent checkout, mismatched refs, wrong commit SHA, and valid paired checkouts.

- [ ] **Step 2: Run tests to verify failure**

Run: `python3 -m pytest tools/tests/test_doctor.py -v`
Expected: tests fail because doctor implementation is absent.

- [ ] **Step 3: Implement validation and fetch commands**

Use the manifest as the only source of truth; reject SDK/solution ref mismatch only for profiles that declare a solution, before invoking Make.

- [ ] **Step 4: Run tests and a dry-run doctor**

Run: `python3 -m pytest tools/tests/test_doctor.py -v` and `./tools/bk doctor --profile bk7259_robot --dry-run`.
Expected: tests pass and dry-run prints the locked pair.

- [ ] **Step 5: Commit**

```bash
git add tools README_CN.md
git commit -m "feat: add upstream version doctor"
```

### Task 3: 建立 `bk_core` 和统一应用生命周期

**Files:**
- Create: `bk-open-framework/components/bk_core/include/bk_core.h`
- Create: `bk-open-framework/components/bk_core/include/bk_err.h`
- Create: `bk-open-framework/components/bk_core/include/bk_event.h`
- Create: `bk-open-framework/components/bk_core/src/bk_core.c`
- Create: `bk-open-framework/components/bk_core/src/bk_event.c`
- Create: `bk-open-framework/components/bk_core/Kconfig`
- Create: `bk-open-framework/components/bk_core/CMakeLists.txt`
- Create: `bk-open-framework/components/bk_core/tests/test_event.c`

**Interfaces:**
- `bk_err_t bk_core_init(const bk_core_config_t *config)`
- `bk_err_t bk_core_start(void)`
- `bk_err_t bk_event_subscribe(bk_event_id_t id, bk_event_handler_t handler, void *ctx)`
- `bk_err_t bk_event_publish(const bk_event_t *event)`
- `bk_err_t bk_core_deinit(void)`

- [ ] **Step 1: Write event ordering and lifecycle tests**
- [ ] **Step 2: Run tests and verify they fail**
- [ ] **Step 3: Implement C API, Kconfig and Armino component integration**
- [ ] **Step 4: Run host tests and BK7259 compile smoke test**
- [ ] **Step 5: Commit**

### Task 4: 建立 `bk_board` 与 `bk7259_robot` 板卡适配层

**Files:**
- Create: `bk-open-framework/components/bk_board/include/bk_board.h`
- Create: `bk-open-framework/components/bk_board/src/bk_board.c`
- Create: `bk-open-framework/boards/bk7259_robot/board.yaml`
- Create: `bk-open-framework/boards/bk7259_robot/Kconfig`
- Create: `bk-open-framework/boards/bk7259_robot/pins.h`
- Create: `bk-open-framework/boards/bk7259_robot/README_CN.md`
- Create: `bk-open-framework/components/bk_board/tests/test_capabilities.c`

**Interfaces:**
- `bk_err_t bk_board_init(const bk_board_config_t *config)`
- `bool bk_board_has_capability(bk_board_capability_t capability)`
- `const bk_board_info_t *bk_board_get_info(void)`
- `bk_err_t bk_board_get_pin(bk_board_pin_id_t id, bk_gpio_pin_t *pin)`

- [ ] **Step 1: Write capability and missing-pin tests**
- [ ] **Step 2: Run tests to verify failure**
- [ ] **Step 3: Encode only verified `bk7259_robot` hardware capabilities and pins**
- [ ] **Step 4: Compile against the locked SDK and verify board metadata**
- [ ] **Step 5: Commit**

### Task 5: 实现 `bk_net`：配网、连接状态和局域网发现

**Files:**
- Create: `bk-open-framework/components/bk_net/include/bk_net.h`
- Create: `bk-open-framework/components/bk_net/include/bk_provision.h`
- Create: `bk-open-framework/components/bk_net/include/bk_lan.h`
- Create: `bk-open-framework/components/bk_net/src/bk_net.c`
- Create: `bk-open-framework/components/bk_net/src/bk_provision.c`
- Create: `bk-open-framework/components/bk_net/src/bk_lan.c`
- Create: `bk-open-framework/components/bk_net/Kconfig`
- Create: `bk-open-framework/components/bk_net/tests/test_net_state.c`

**Interfaces:**
- `bk_err_t bk_net_start(const bk_net_config_t *config)`
- `bk_net_state_t bk_net_get_state(void)`
- `bk_err_t bk_provision_start(const bk_provision_config_t *config)`
- `bk_err_t bk_lan_start(const bk_lan_config_t *config)`
- `bk_err_t bk_lan_publish(const bk_device_record_t *record)`

- [ ] **Step 1: Write state-machine tests for connect, disconnect, retry and reprovision**
- [ ] **Step 2: Run tests to verify failure**
- [ ] **Step 3: Implement adapters over Armino Wi-Fi/BLE and existing LAN discovery code**
- [ ] **Step 4: Run host state tests and BK7259 hardware smoke test**
- [ ] **Step 5: Commit**

### Task 6: 迁移音视频和 UI 为可选框架组件

**Files:**
- Create: `bk-open-framework/components/bk_media/include/bk_media.h`
- Create: `bk-open-framework/components/bk_ui/include/bk_ui.h`
- Create: corresponding `src/`, `Kconfig`, `CMakeLists.txt`, and README files
- Modify: `bk-open-framework/platforms/bk7259_robot.yaml`

**Interfaces:**
- Media and UI APIs must depend on `bk_core`/`bk_board`, not on product-specific Agora or robot headers.
- Every component must have an explicit Kconfig switch and a minimal compile target.

- [ ] **Step 1: Add component API compile tests with all optional features disabled**
- [ ] **Step 2: Run tests and verify missing component targets fail**
- [ ] **Step 3: Implement thin adapters over the SDK; only the robot profile may reuse `bk_solution_ai` components**
- [ ] **Step 4: Build minimal and full-feature configurations separately**
- [ ] **Step 5: Commit**

### Task 7: 实现 OTA 薄适配层

**Files:**
- Create: `bk-open-framework/components/bk_ota/include/bk_ota.h`
- Create: `bk-open-framework/components/bk_ota/src/bk_ota.c`
- Create: `bk-open-framework/components/bk_ota/Kconfig`
- Create: `bk-open-framework/components/bk_ota/tests/test_ota_state.c`

**Interfaces:**
- `bk_ota_start(const bk_ota_config_t *config)` wraps the SDK OTA path only.
- The first version documents that signing keys, rollback policy and production security remain SDK/product responsibilities.

- [ ] **Step 1: Write OTA state tests and unsupported-security-mode tests**
- [ ] **Step 2: Implement the thin SDK adapter**
- [ ] **Step 3: Run compile smoke and state tests**
- [ ] **Step 4: Commit**

### Task 8: 实现外设公共接口

**Files:**
- Create: `bk-open-framework/components/bk_peripheral/include/bk_peripheral.h`
- Create: `bk-open-framework/components/bk_peripheral/src/bk_peripheral.c`
- Create: `bk-open-framework/components/bk_peripheral/Kconfig`
- Create: `bk-open-framework/components/bk_peripheral/tests/test_peripheral_caps.c`

**Interfaces:**
- Peripheral APIs depend on `bk_core`/`bk_board`; missing capabilities return explicit errors.

- [ ] **Step 1: Write capability and missing-device tests**
- [ ] **Step 2: Implement thin adapters for verified `bk7259_robot` peripherals**
- [ ] **Step 3: Run compile smoke and board capability tests**
- [ ] **Step 4: Commit**

### Task 9: 迁移 `bk7259_robot` 参考产品并隔离专有插件

**Files:**
- Create: `bk-open-framework/products/bk7259_robot/CMakeLists.txt`
- Create: `bk-open-framework/products/bk7259_robot/Kconfig`
- Create: `bk-open-framework/products/bk7259_robot/main.c`
- Create: `bk-open-framework/products/bk7259_robot/plugins/agora_adapter.h`
- Create: `bk-open-framework/products/bk7259_robot/plugins/README_CN.md`
- Reference only: existing `bk_solution_ai` product sources used as migration input; do not modify upstream repository

**Interfaces:**
- Product main consumes only framework public headers.
- `agora_adapter.h` defines an optional interface; core product builds without Agora.

- [ ] **Step 1: Write a dependency inventory and no-plugin minimum-boot spike**
- [ ] **Step 2: Define the smallest product configuration that boots without Agora**
- [ ] **Step 3: Move only initialization, network, board and event usage behind framework APIs**
- [ ] **Step 4: Build no-plugin and plugin-enabled configurations; run the supported path on `bk7259_robot`**
- [ ] **Step 5: Commit**

The no-plugin configuration promises only boot, board initialization and basic networking; it does not promise complete robot functionality.

### Task 10: 分里程碑完成示例、文档、CI 和发布验收

**Files:**
- Create: `bk-open-framework/examples/01_wifi_provision/`
- Create: `bk-open-framework/examples/02_lan_device/`
- Create: `bk-open-framework/examples/03_audio_player/`
- Create: `bk-open-framework/examples/04_camera_stream/`
- Create: `bk-open-framework/examples/05_peripheral_control/`
- Create: `bk-open-framework/examples/06_robot_reference/`
- Create: `bk-open-framework/.github/workflows/build.yml`
- Create: `bk-open-framework/docs/getting-started/README_CN.md`
- Create: `bk-open-framework/docs/migration/bk_solution_ai.md`
- Create: `bk-open-framework/docs/release-checklist.md`

**Interfaces:**
- Each example declares its platform and enabled components.
- CI runs manifest checks, host tests, minimal configuration build and full configuration build.

- [ ] **Step 1: Deliver M1 examples (`01_wifi_provision`, `02_lan_device`) and minimal build checks**
- [ ] **Step 2: Deliver M2 media/UI/peripheral/OTA examples and compile checks**
- [ ] **Step 3: Deliver M3 robot reference and optional plugin documentation**
- [ ] **Step 4: Run hardware acceptance as a release gate, separately from ordinary CI; record PASS criteria for boot, provisioning, LAN discovery, display, audio, camera, OTA and each verified peripheral in `docs/release-checklist.md`**
- [ ] **Step 5: Tag framework `v0.1.0` with the upstream pair and evidence recorded in the release checklist**
