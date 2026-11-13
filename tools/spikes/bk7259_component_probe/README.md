# BK7259 AP/CP component probe

该 spike 用于验证外部框架组件如何接入 BK7259 AP/CP 构建。

## 通过条件

1. 空组件在 AP 构建侧成功编译；
2. 空组件在 CP 构建侧成功编译；
3. 两侧产物被正确打包；
4. 最终 `all-app.bin` 链接成功；
5. Kconfig 开关关闭时组件不进入对应镜像。

当前状态：**已验证（PASS）**。已基于锁定版本的 `bk_avdk_smp`（`584b947920c414230f9909375f008e7d6a2c6c6b`）、`bk_solution_ai`（`a1e865e156e8dc31dfb360219c33a6a6d908ea98`）与官方 `arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi` 工具链完成 AP/CP 双子系统交叉编译与 `all-app.bin` 固件打包验证。

## 验证记录

- AP 工程入口：`bk_solution_ai/projects/beken_robot/Makefile` 与 `CMakeLists.txt`（子系统入口 `ap/CMakeLists.txt`）。
- CP 工程入口：`bk_solution_ai/projects/beken_robot/cp/CMakeLists.txt`（GPIO 配置 `cp/config/bk7259/usr_gpio_cfg.h`）。
- 注入组件：`bk_open_framework`（由 `tools/spikes/bk7259_component_probe/bk_open_framework.cmake` 注册，分别生成 `bk7259_ap/armino/bk_open_framework/libbk_open_framework.a` 与 `bk7259/armino/bk_open_framework/libbk_open_framework.a` 并链接进 `app.elf`）。
- 目标产物：`projects/beken_robot/build/bk7259/beken_robot/package/all-app.bin`（SHA256：`ba95ad08219eaea0498296556774c7699cc8d3f01c96064023bf23cdfd7e4501`）。
