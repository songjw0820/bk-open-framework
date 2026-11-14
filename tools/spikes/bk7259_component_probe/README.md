# BK7259 AP/CP component probe

该 spike 用于验证外部框架组件如何分别在 `minimal`（纯 SDK `projects/app`）与 `robot`（`bk_solution_ai/projects/beken_robot`）两种 profile 下接入 BK7259 AP/CP 构建，并验证 Kconfig 开关与 ELF 符号裁剪。

## 通过条件

1. 探针组件（`fixture/bk_probe_comp`，导出符号 `bk_open_framework_probe_marker`）注入到 AP（`${PROJECT_DIR}/ap/components/bk_probe_comp`）和 CP（`${PROJECT_DIR}/cp/components/bk_probe_comp`）；
   - **重要机制发现**：依据 `bk_avdk_smp/tools/build_tools/cmake/project.cmake:266-277`，`EXTRA_COMPONENTS_DIRS` 仅在 `sub_sys == "ap"` 时生效；而 `${PROJECT_DIR}/${sub_sys}/components` 在 `ap` 与 `cp` 两侧均会被自动扫描。
2. 当 `CONFIG_BK_OPEN_FRAMEWORK_PROBE=y`（`--config-mode on`）时，AP (`bk7259_ap`) 与 CP (`bk7259`) 均成功编译链接，生成的 `app.elf` 中经 `nm` 确认存在符号 `bk_open_framework_probe_marker`，且成功打包生成 `<SDK_DIR>/build/bk7259/<project>/package/all-app.bin`；
3. 当 `# CONFIG_BK_OPEN_FRAMEWORK_PROBE is not set`（`--config-mode off`）时，AP 与 CP 产物经 `nm` 确认**不存在**符号 `bk_open_framework_probe_marker`，且成功打包生成 `all-app.bin`；
4. `minimal` profile 仅使用 `bk_avdk_smp/projects/app`，不依赖 `bk_solution_ai`。

当前状态：**探针脚本与组件 Fixture 已就绪；完整编译待在工具链环境中执行（`not_run_toolchain_unavailable`）**。
在 Task 0 实测通过前，不允许进入正式组件代码实现。
