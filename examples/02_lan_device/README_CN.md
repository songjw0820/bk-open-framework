# M1 局域网设备示例

演示设备身份和局域网发现入口。发现协议由 bk_lan 适配器注入。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`minimal`
- 里程碑：`M1`
- 启用组件：``bk_core`, `bk_board`, `bk_net``
- SDK 边界：host 可检查，目标固件需要 Armino SDK
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_minimal.yaml
cmake -S examples/02_lan_device -B /tmp/bk-02_lan_device
cmake --build /tmp/bk-02_lan_device
/tmp/bk-02_lan_device/02_lan_device
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
