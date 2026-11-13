# `bk_solution_ai` 迁移边界

本文只记录外部工程的路径、接口和插件边界，不复制其源代码、资源或许可证文本。`bk_solution_ai` 仍是独立 checkout，框架只通过 profile 清单和产品集成使用它。

## 外部路径

- 依赖根目录由 `./tools/bk env --profile bk7259_robot --root <deps>` 输出。
- SDK 路径为 `<deps>/bk_avdk_smp`，由 `platforms/bk7259_robot.yaml` 锁定 `release/v4.0.1.13` 与 SHA。
- solution 路径为 `<deps>/bk_solution_ai`，由同一清单锁定匹配 ref 与 SHA。
- 框架产品入口位于 `products/bk7259_robot/`。公共组件位于 `components/`，不从 solution 目录复制实现。

## 接口边界

- 产品启动只依赖 `bk_core`、`bk_board` 和 `bk_net` 的公共 C API。
- 媒体、UI、OTA 和外设能力通过各自的适配器接口注入，适配器负责把 SDK 或 solution 的设备实现连接到公共 API。
- `products/bk7259_robot/plugins/agora_adapter.h` 只声明 opaque context 和生命周期/网络状态回调。Agora、RTSA、RTM 头文件、库、密钥和链接参数由产品集成提供。
- `bk_solution_ai` 的业务流程、模型、云端资源和专有协议不进入 minimal profile，也不成为公共组件的编译依赖。

## 迁移步骤

1. 使用 `bk7259_robot` 清单获取并诊断两个外部 checkout，确认 ref 和 SHA 成对匹配。
2. 在产品集成层实现媒体、UI、OTA、外设或云端插件适配器，公共组件只接收定义好的 callback 和数据结构。
3. 保留 solution 的构建脚本、资源和许可声明在其原 checkout 或产品发布包中。提交框架时只提交路径说明、接口声明和验证证据。
4. 先运行 host CMake 测试，再运行 SDK 原生 AP/CP 构建和 `all-app.bin` 打包。硬件结果按发布清单登记。

## 不纳入框架仓库的内容

solution 源文件、模型和媒体资源、生成的固件、Agora/AI 云端 SDK、访问凭据以及对应专有许可证文本都不复制到本仓库。发布前由集成方单独确认这些外部依赖的授权和分发方式。
