# M2 外设控制示例

演示芯片无关外设查询和打开边界。仅 cp_uart0 已在 board.yaml 标记为 verified。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M2`
- 启用组件：`bk_core`、`bk_board`、`bk_peripheral``
- SDK 边界：需要 Armino SDK 的 GPIO、UART 和具体外设驱动
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/05_peripheral_control -B /tmp/bk-05_peripheral_control
cmake --build /tmp/bk-05_peripheral_control
/tmp/bk-05_peripheral_control/05_peripheral_control
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
