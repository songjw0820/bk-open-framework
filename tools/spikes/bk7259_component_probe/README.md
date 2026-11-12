# BK7259 AP/CP component probe

该 spike 用于验证外部框架组件如何接入 BK7259 AP/CP 构建。

## 通过条件

1. 空组件在 AP 构建侧成功编译；
2. 空组件在 CP 构建侧成功编译；
3. 两侧产物被正确打包；
4. 最终 `all-app.bin` 链接成功；
5. Kconfig 开关关闭时组件不进入对应镜像。

当前状态：**未执行**。本工作区没有锁定版本的 `bk_avdk_smp` checkout 和完整 Armino 工具链，因此不能声称 AP/CP 或 `all-app.bin` 已验证。

## 记录的入口（待工具链验证）

- AP 工程入口：`bk_solution_ai/projects/beken_robot/Makefile` 与 `CMakeLists.txt`。
- CP 工程目录：`bk_solution_ai/projects/beken_robot/cp/`，其 BK7259 GPIO 声明位于 `cp/config/bk7259/usr_gpio_cfg.h`。
- 目标产物：`projects/beken_robot/build/bk7259/beken_robot/package/all-app.bin`（README 记录的路径）。
- 下一步：使用匹配的 `bk_avdk_smp` checkout，分别构建 AP/CP 空组件，并记录命令、日志和产物摘要。
