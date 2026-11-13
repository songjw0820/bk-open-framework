# M2 摄像头流示例

演示视频帧提交和网络发送的公共边界，不携带摄像头驱动或专有云 SDK。

- 平台：`BK7259`，板卡：`bk7259_robot`
- profile：`robot`
- 里程碑：`M2`
- 启用组件：``bk_core`, `bk_board`, `bk_media`, `bk_net``
- SDK 边界：需要 Armino SDK 的摄像头驱动、编码器和网络发送适配
- 可选插件：无

## 无 SDK 主机检查

```bash
python3 tools/manifest.py platforms/bk7259_robot.yaml
cmake -S examples/04_camera_stream -B /tmp/bk-04_camera_stream
cmake --build /tmp/bk-04_camera_stream
/tmp/bk-04_camera_stream/04_camera_stream
```

以上命令只验证清单和示例入口。目标板上的驱动、固件打包和硬件验收按 `docs/release-checklist.md` 执行。
