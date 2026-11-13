# M2 OTA 更新示例

演示 `bk_ota` 的下载、校验、应用和失败恢复边界。签名密钥、回滚策略和生产安全由 SDK 或产品集成负责，当前硬件验收仍待执行。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M2`
- 启用组件：`bk_core`、`bk_ota`
- SDK 边界：需要 Armino SDK 的 OTA 下载、签名校验和回滚策略
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/08_ota_update -B /tmp/bk-08_ota_update
cmake --build /tmp/bk-08_ota_update
/tmp/bk-08_ota_update/08_ota_update
```

以上命令只验证清单和示例入口。目标板 OTA 验收按 `docs/release-checklist.md` 执行。
