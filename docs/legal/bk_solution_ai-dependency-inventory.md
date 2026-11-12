# bk_solution_ai 依赖清单（Task 0 初版）

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

## 当前阻塞

本清单不是法律结论。未完成许可证文件、模型、音频资源和第三方二进制的逐项审计前，不将相关文件复制到公共框架仓库。
