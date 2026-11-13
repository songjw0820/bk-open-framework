# M3 机器人参考示例

组合网络、媒体、UI、OTA 和外设公共接口。Agora/AI 能力只通过可选插件边界接入。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M3`
- 启用组件：`bk_core`、`bk_board`、`bk_net`、`bk_media`、`bk_ui`、`bk_ota`、`bk_peripheral`
- SDK 边界：需要锁定的 Armino SDK；完整机器人固件还需要外部 bk_solution_ai 参考工程
- 可选插件：agora_adapter（可选，需由产品集成注入）

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/06_robot_reference -B /tmp/bk-06_robot_reference
cmake --build /tmp/bk-06_robot_reference
/tmp/bk-06_robot_reference/06_robot_reference
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
