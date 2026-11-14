# BK Open Framework 架构深度评审与演进规范（Rev 4 · 方案复审通过，Task 0 阻塞项与待验证项已锁定）

- **文档路径**：`docs/reviews/2026-11-13-architecture-review-and-espressif-comparison.md`
- **当前评级状态**：**架构方向可继续；实施放行仍阻塞于 Host 依赖、锁定 SHA 目标构建、实机验证和 OTA 密码学验收**
- **唯一事实来源同步文件**：
  - 设计规范：[docs/superpowers/specs/2026-11-12-bk-open-framework-design.md](../superpowers/specs/2026-11-12-bk-open-framework-design.md)
  - 实施计划：[docs/superpowers/plans/2026-11-12-bk-open-framework-implementation.md](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md)
  - 平台证据：[docs/platforms/bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md)
  - 发布验收清单（空门禁模板）：[docs/release-checklist.md](../release-checklist.md)
  - 平台清单：[platforms/bk7259_minimal.yaml](../../platforms/bk7259_minimal.yaml)、[platforms/bk7259_robot.yaml](../../platforms/bk7259_robot.yaml)
  - 构建探针：[tools/spikes/bk7259_component_probe/probe.sh](../../tools/spikes/bk7259_component_probe/probe.sh)

---

## 零、 Codex 针对 `2dac28a` 复审意见的修正与状态跟踪（Rev 3 -> Rev 4）

| 级别 | Codex 指出的问题（针对 `2dac28a`） | Rev 4 已完成的仓库修正 | 当前验证状态 |
| :--- | :--- | :--- | :--- |
| **P0-1** | **`probe.sh` 的产物路径错误**：`make -C "$sdk_dir"` 时，`tools/build_main.mk` 将 `PROJECT_BUILD_DIR` 设为 `$(CURDIR)/build/bk7259/$project_name`，产物位于 `$sdk_dir/build/bk7259/$project_name/package/all-app.bin`，而非 `$project_dir/build/...`。 | 已修正 [probe.sh](../../tools/spikes/bk7259_component_probe/probe.sh) 第 77 行及 [README.md](../../tools/spikes/bk7259_component_probe/README.md)，统一指向 `$sdk_dir/build/bk7259/$project_name/package/all-app.bin`。 | **脚本路径已修复；待工具链环境实测 `--build`** |
| **P0-2** | **`bk7259_minimal` 虽已隔离 CP 模板来源，但尚未证明可独立构建**：状态仍为 `not_run_toolchain_unavailable`。 | 在全仓文档中明确将 **`minimal` 与 `robot` 两次 `probe.sh --build` 实测通过** 列为启动正式组件代码实现的**前置阻塞门禁（Blocking Gate）**。 | **待在挂载锁定 SDK 与 `cm55`/`cm52` 工具链的环境实测** |
| **P1-1** | **OTA 实施计划仍依赖未完成的分区重算**：在提交具体分区 CSV、实测 AP/CP 镜像大小、OTA 压缩包大小、BL2 配置及 `OVERWRITE` vs `XIP` 选型前，OTA 不能进入默认组件交付。 | 已在 [bk7259_robot.yaml](../../platforms/bk7259_robot.yaml) 中将 `bk_ota` 从 `components` 移入 `deferred_target_components`，并在 [实施计划](../superpowers/plans/2026-11-12-bk-open-framework-implementation.md) 与 [release-checklist.md](../release-checklist.md) 中明确列出 5 项前置交付物门禁。 | **已移出默认组件交付；待 5 项分区与尺寸证据提交后解封** |
| **P1-2** | **静态 Cache/DMA 证据需避免过度推断外设行为**：SDK API 存在不代表摄像头/LCD/JPEG 外设 DMA 访问对应 PSRAM 区域的行为已验证。 | 已在 [bk7259_v4.0.1.11.md](../platforms/bk7259_v4.0.1.11.md) 第 3.1 节与本规范中严格拆分**“已核实的 SDK 静态 API 事实”**与**“待 Tier 2/4 实测的外设 DMA PSRAM 行为”**，在实测前不固化外设 ownership 结论。 | **边界已严格切分；待实机验证** |
| **P1-3** | **发布清单目前只是空门禁**：所有项未勾选，无实际日志，不构成通过证据。 | 已在 [docs/release-checklist.md](../release-checklist.md) 顶部显式声明：`Empty Gate Template — NOT YET VERIFIED (0 项已验证)`。 | **文档定性已明确** |
| **P2-1** | **版本字段命名歧义**：同时保留 `tag_sha`, `commit_sha`, `sha` 易混淆 tag object 与工作树 commit。 | 已在 [bk7259_minimal.yaml](../../platforms/bk7259_minimal.yaml) 与 [bk7259_robot.yaml](../../platforms/bk7259_robot.yaml) 中删除通用 `sha`/`tag_sha`/`commit_sha`，只保留无歧义的 `tag_object_sha` 与 `peeled_commit_sha`。 | **已修复** |

---

## 一、 当前已锁定的架构共识（方案复审已通过项）

1. **阶段范围**：Phase 1/2 唯一目标平台为 **`bk7259_robot`（BK7259, `release/v4.0.1.11`）**，仅实现 `bk7259_v4` 与 `posix_sim` 后端；BK7258 (`release/v3.1.1.x`) 归入 Phase 3。
2. **Minimal 与 Solution 的 CP 模板分源隔离**：
   - `bk7259_minimal`：仅依赖 `bk_avdk_smp`（`peeled_commit_sha: 584b947920c414230f9909375f008e7d6a2c6c6b`），CP 模板取自 `bk_avdk_smp/projects/app/cp`，产物路径为 `<SDK_DIR>/build/bk7259/app/package/all-app.bin`。
   - `bk7259_robot`：额外锁定 `bk_solution_ai`（`peeled_commit_sha: a1e865e156e8dc31dfb360219c33a6a6d908ea98`），CP 模板取自 `bk_solution_ai/projects/beken_robot/cp`，产物路径为 `<SDK_DIR>/build/bk7259/beken_robot/package/all-app.bin`。
3. **细粒度组件拆分（机制层 vs 暂缓目标组件 vs 产品策略层）**：
   - **默认机制层组件（`components/`）**：`bk_common`、`bk_port`、`bk_core_event`、`bk_storage`、`bk_health`、`bk_board`、`bk_net`、`bk_button`、`bk_led_effect`、`bk_audio_session`（固定 20ms PCM 帧）、`bk_audio_mixer`、`bk_video_frame_pool`（64B 对齐）、`bk_ui_core`、`bk_ai_agent`（拆分为 `transport_websocket` + `protocol_xiaozhi`，内含 3×20ms->60ms Opus 打包器，以 MCP JSON-RPC 2.0 为唯一主路径，`iot` 标为 deprecated 默认关闭）。
   - **暂缓目标组件（`deferred_target_components`）**：`bk_ota`（待完成 5 项分区重算与尺寸/BL2 证据后解封）。
   - **产品策略组件（`product_components/`）**：`bk_motion`、`bk_robot_face`、`bk_battery_policy`。
   - **可选商业插件（`plugins/`）**：`plugin_agora`。
4. **静态 SDK 事实与待验证硬件行为严格分离**：
   - **已核实的 SDK 源码事实**：PL310 32B cache line (`l2_cache.c`)、64B `BK_FRAME_QUEUE_ALIGN_SIZE` (`bk_frame_queue.h`)、`bk_dcache_addr_is_cacheable` / `bk_dcache_clean_for_producer` / `bk_dcache_invalidate_for_consumer` (`cache.c`)、`psram_malloc` 及 PSRAM slab 划分 (`mem.h` / `ram_regions.csv`)、BL2 EC-P256 签名与 `beken_robot` 8MB 分区表 `ota = 264K` 限制。
   - **待实测验证的硬件行为**：摄像头/JPEG/LCD 外设 DMA 在 `bk7259_robot` 上对 Cached/Uncached PSRAM Slab 的实际访问约束与 ownership 切换时序，需在 Tier 2/Tier 4 实机测试中验证后写入板级契约。

---

## 二、 当前仓库实施进展与门禁状态（Rev 5 复核基线）

1. **Tier 1 Host 行为门禁已在补齐依赖后通过**：在开启 `-Wall -Wextra -Werror -fsanitize=address,undefined` 的构建目录中，使用 PyYAML 6.0.3 和现有 libopus 动态库完成 `15/15` CTest（含全组件 `BK_OF_BUILDING_TARGET_BK7259=1` 目标适配层编译验证）与 `15/15` Python 单测。未设置这两个依赖时，协议测试和 Python 测试会失败，验证环境必须记录依赖来源。
2. **乐鑫组件生态兼容层（`bk_esp_compat`）**：已交付 [`components/bk_esp_compat`](../../components/bk_esp_compat/) 与 [`docs/espressif-component-porting-guide.md`](../espressif-component-porting-guide.md)，提供经过 Host 单测覆盖的 ESP-IDF API 子集。`esp_timer`、`esp_event`、NVS、按键和 LED 的运行时语义仍需按具体组件版本补充验证，不能据此承诺所有 Registry 组件零修改移植。
3. **OTA 安全边界**：`bk_ota` 在未提供 `sig_verify_fn` 时现在返回 `BK_ERR_NOT_SUPPORTED`，不会把非零字节当作 EC-P256 签名通过。真正的 mbedTLS/SDK EC-P256 验证、公钥来源和签名覆盖范围仍由目标集成提供。
4. **Tier 2 ~ Tier 4 剩余硬件门禁**：已在 Docker `bekencorp/armino-idk:1.5` 中使用精确锁定 SHA 完成 `minimal` 和 `robot` 两次 `probe.sh --build`，AP/CP ELF、Kconfig on/off 符号裁剪和 `all-app.bin` 均通过，产物大小与 SHA256 已写入平台清单。仍需在 `bk7259_robot` 实机完成外设 DMA、音频、WSS 与 OTA 验收。
