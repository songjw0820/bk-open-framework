# bk_solution_ai 依赖清单（Task 0 初版）

范围：仅记录依赖边界和审计入口，不构成许可证法律意见。

## 目标

确认哪些内容可以进入 Apache-2.0 公共框架，哪些必须保留为 robot profile 或外部插件。

## 初步分类

| 类别 | 处理方式 |
|---|---|
| Armino SDK 的 Wi-Fi/BLE/RTOS/驱动/编解码 | 由外部 SDK 提供，不复制 |
| `bk_solution_ai` 中的通用网络、事件、媒体、板卡代码 | 逐项审计后抽取到框架 |
| Agora RTC 和云端 AI SDK | 可选插件，不进入核心依赖 |
| 音频资源、NPU 模型、第三方二进制 | 检查 LICENSE/NOTICE 和再分发限制 |
| 机器人产品 UI 和业务流程 | 保留在 `bk7259_robot` 产品 profile |

## Profile 约束

- `bk7259_minimal` 的 `solution` 必须为 `null`，只依赖外置 Armino SMP SDK。
- `bk7259_robot` 才允许声明 `bk_solution_ai`，并要求与 SDK 使用相同的 `release/v4.0.1.11` ref。
- 两个 profile 的 SDK/solution SHA 当前均为待上游 checkout 验证的占位值；不得把本地工作副本 SHA 当作发布锁定 SHA。

## 当前阻塞

本清单不是法律结论。未完成许可证文件、模型、音频资源和第三方二进制的逐项审计前，不将相关文件复制到公共框架仓库。

## 静态审计入口

- `bk_solution_ai/LICENSE`、`docs/LICENSE`：仓库级许可证文件。
- `projects/beken_robot/`：产品工程、AP/CP 构建入口和板级配置。
- `components/*/Kconfig`、`components/*/CMakeLists.txt`：组件注入点与潜在第三方链接依赖。
- `README.md`：标注 Agora RTC、AI Agent、OPUS/H264/JPEG 等可选能力；是否可再分发需逐项查阅随附 LICENSE/NOTICE。
