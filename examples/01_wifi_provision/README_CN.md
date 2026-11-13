# M1 最小配网示例

演示最小启动、配网状态和网络状态入口。示例不引用机器人 solution。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`minimal`
- 里程碑：`M1`
- 启用组件：``bk_core`, `bk_board`, `bk_net``
- SDK 边界：host 可检查，目标固件需要 Armino SDK
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml
cmake -S examples/01_wifi_provision -B /tmp/bk-01_wifi_provision
cmake --build /tmp/bk-01_wifi_provision
/tmp/bk-01_wifi_provision/01_wifi_provision
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
