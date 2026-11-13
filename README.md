# BK Open Framework

面向 BK7259 模组应用开发的开源框架，建立在 Beken Armino SMP 之上。

第一阶段只支持 BK7259 的 `bk7259_robot` 板卡和 `minimal`/`robot` 两种 profile，锁定：

- Armino SMP `release/v4.0.1.11`
- `bk_solution_ai` `release/v4.0.1.11`

仓库内容包括：

- `platforms/`：芯片、板卡和上游 SDK 版本清单
- `components/`：后续抽取的公共组件
- `tools/`：版本检查和构建工具
- `docs/`：设计文档、实施计划、平台证据和依赖审计

平台清单输出 `chip`、`board`、`profile`、SDK 的 `repository`/`ref`/`sha`，以及可选 solution 的对应字段。每个依赖必须固定到 40 位 Git SHA；存在 solution 时，它与 SDK 必须使用同一个 release ref。可在未安装 SDK 工具链的环境中运行清单校验：

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
```

上游 SDK 和 solution 保持外置，通过 profile 清单锁定版本。`bk7259_minimal` 只依赖 SDK，`bk7259_robot` 额外使用机器人 solution。其他芯片、板卡和 profile 当前不在支持范围内。

当前 Task 0 已完成静态平台证据和 AP/CP 构建 probe。完整 SDK 工具链和实机编译验证将在后续执行。
