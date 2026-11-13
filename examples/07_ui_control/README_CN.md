# M2 UI 控制示例

演示 `bk_ui` 的显示渲染和输入事件适配边界。板级显示和输入驱动由 Armino SDK 注入，当前硬件能力仍待验收。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M2`
- 启用组件：`bk_core`、`bk_board`、`bk_ui`
- SDK 边界：需要 Armino SDK 的显示和输入驱动
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/07_ui_control -B /tmp/bk-07_ui_control
cmake --build /tmp/bk-07_ui_control
/tmp/bk-07_ui_control/07_ui_control
```

以上命令只验证清单和示例入口。目标板显示验收按 `docs/release-checklist.md` 执行。
