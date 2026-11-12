# BK7259 AP/CP component probe

该 spike 用于验证外部框架组件如何接入 BK7259 AP/CP 构建。

## 通过条件

1. 空组件在 AP 构建侧成功编译；
2. 空组件在 CP 构建侧成功编译；
3. 两侧产物被正确打包；
4. 最终 `all-app.bin` 链接成功；
5. Kconfig 开关关闭时组件不进入对应镜像。

当前状态：**未执行**。本工作区没有锁定版本的 `bk_avdk_smp` checkout 和完整 Armino 工具链，因此不能声称 AP/CP 或 `all-app.bin` 已验证。
