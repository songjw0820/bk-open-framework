# M2 音频播放器示例

演示可选媒体组件的音频输入输出边界。当前板卡音频能力仍需硬件验收。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M2`
- 启用组件：`bk_core`、`bk_board`、`bk_media``
- SDK 边界：需要 Armino SDK 的音频驱动和板级适配
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/03_audio_player -B /tmp/bk-03_audio_player
cmake --build /tmp/bk-03_audio_player
/tmp/bk-03_audio_player/03_audio_player
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
