# Agora 可选插件边界

`agora_adapter.h` 只定义 opaque context 和回调，不包含 Agora、RTSA 或 RTM 的专有头文件。默认配置 `BK7259_ROBOT_AGORA=n`，产品主程序不会引用或链接专有 SDK，传入 adapter 会返回 `BK_ERR_NOT_SUPPORTED`。

启用 `BK7259_ROBOT_AGORA=y` 或 host CMake 的 `-DBK7259_ROBOT_ENABLE_AGORA=ON` 后，集成方必须显式提供 `init`、`on_network_state` 和可选 `deinit` 回调，并负责将专有 SDK 头文件、库、授权和链接参数注入 Armino 产品构建。adapter 的 `context` 由集成方拥有，回调返回错误时启动或网络状态传播立即停止。

无插件产品只承诺 core init/start、`bk7259_robot` board init、基础网络 start 和连接回调。完整机器人 UI、媒体、AI Agent、Agora RTC/RTSA/RTM、模型和资源仍属于外置 profile 或专有集成范围。

来源边界参考 `docs/legal/bk_solution_ai-dependency-inventory.md` 和外置 `bk_solution_ai` checkout。迁移只读参考上游，不复制或修改上游源码、二进制、资源或许可证文件。
