# BK Open Framework

BK Open Framework 面向 BK7259 模组应用开发，构建于 Beken Armino SMP 之上。

第一阶段只支持 BK7259 的 `bk7259_robot` 板卡和 `minimal`、`robot` 两种 profile，平台清单位于 `platforms/`：

- `bk7259_minimal.yaml`：只锁定 Armino SMP SDK。
- `bk7259_robot.yaml`：额外锁定 `bk_solution_ai`。

每个清单都固定上游仓库、release ref 和 40 位 Git SHA。可使用以下命令检查清单：

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
```

公共组件放在 `components/`，板卡适配放在 `boards/`，示例和产品集成分别放在
`examples/` 与 `products/`，工具位于 `tools/`。SDK 与 solution 保持外置，其他芯片、板卡和 profile 当前不在支持范围内，完整工具链和硬件验证结果将由后续任务补充。
