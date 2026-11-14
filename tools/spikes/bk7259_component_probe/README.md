# BK7259 AP/CP component probe

该 spike 用于验证外部框架组件如何分别在 `minimal`（纯 SDK）与 `robot`（含 `bk_solution_ai`）两种 profile 下接入 BK7259 AP/CP 构建。

## 通过条件

1. 空组件在 AP 构建侧成功编译；
2. 空组件在 CP 构建侧成功编译；
3. 两侧产物被正确打包；
4. 最终 `all-app.bin` 链接成功；
5. Kconfig 开关关闭时组件不进入对应镜像；
6. `minimal` profile 仅使用 `bk_avdk_smp/projects/app`，不依赖 `bk_solution_ai`。

当前状态：**未执行完整编译（`not_run_toolchain_unavailable`）**。本工作区没有完整 checkout 的 `bk_avdk_smp` 与 Armino 工具链（AP `cm55` + CP `cm52`），因此不能声称 AP/CP 或 `all-app.bin` 已完成编译验证。在 Task 0 实测通过前，不允许进入正式组件代码实现。

## 记录的入口与产物路径（依据 `bk_avdk_smp/tools/build_main.mk` 核实）

当通过 `make -C <SDK_DIR> bk7259 PROJECT=<project_name> PROJECT_DIR=<project_dir>`（未传 `BUILD_DIR`）调用时，`tools/build_main.mk` 将 `PROJECT_BUILD_DIR` 设为 `$(CURDIR)/build/$(ARMINO_SOC_NAME)/$(PROJECT_NAME)`，因此打包产物固定位于 `<SDK_DIR>/build/bk7259/<project_name>/package/all-app.bin`：

- **`minimal` profile（纯 SDK，无 `bk_solution_ai` 依赖）**：
  - 工程入口：`<SDK_DIR>/projects/app/Makefile` 与 `CMakeLists.txt`
  - AP 入口：`<SDK_DIR>/projects/app/ap/CMakeLists.txt`
  - CP 受控模板：`<SDK_DIR>/projects/app/cp/CMakeLists.txt` 与 `cp/config/bk7259/usr_gpio_cfg.h`
  - 目标产物：`<SDK_DIR>/build/bk7259/app/package/all-app.bin`
- **`robot` profile（依赖 `bk_solution_ai`）**：
  - 工程入口：`<SOLUTION_DIR>/projects/beken_robot/Makefile` 与 `CMakeLists.txt`
  - AP 入口：`<SOLUTION_DIR>/projects/beken_robot/ap/CMakeLists.txt`
  - CP 受控模板：`<SOLUTION_DIR>/projects/beken_robot/cp/CMakeLists.txt` 与 `cp/config/bk7259/usr_gpio_cfg.h`
  - 目标产物：`<SDK_DIR>/build/bk7259/beken_robot/package/all-app.bin`
