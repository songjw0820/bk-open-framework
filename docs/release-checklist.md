# v0.1.0 发布验收清单

状态以证据为准。`PASS` 必须附日志、构建产物、照片或视频的路径和时间戳，`PENDING` 表示当前没有完成该门禁。本清单不创建 Git tag，也不代替 SDK 或硬件团队的发布审批。

## 版本和 upstream pair

- 框架版本：待发布 `v0.1.0`
- SDK upstream pair：`bekencorp/bk_avdk_smp`，`release/v4.0.1.11`，`24cce68adccebec25a7c3bb22e0d1de00f0da5cb`
- solution upstream pair：`bekencorp/bk_solution_ai`，`release/v4.0.1.11`，`009c95182de07ae874e39e3d3c24994431bcec70`
- Tag 规则：框架 tag 使用 `v0.1.0`，发布记录必须同时写入上述 upstream ref 和 40 位 SHA。SDK 与 solution 的 ref 不一致时不得发布。
- 当前 tag 状态：`PENDING`，未执行创建或推送。

## M1 门禁

| 项目 | PASS 证据要求 | 状态 | 证据 |
| --- | --- | --- | --- |
| 清单 | 两个平台清单校验通过，记录命令和输出 | PASS | CI manifest job |
| host 示例 | `01_wifi_provision`、`02_lan_device` 可配置、编译并运行 | PASS | CI minimal-configuration job |
| Boot | 目标板上电后串口日志包含 SDK、板卡和应用启动完成 | PENDING | 待硬件日志 |
| 配网 | 首次配网、错误凭据重试、重启后恢复，各有串口日志和手机/控制端记录 | PENDING | 待硬件记录 |
| LAN discovery | 至少一台 LAN client 发现设备并完成一次命令/状态往返，保存抓包或客户端日志 | PENDING | 待 LAN client 记录 |

## M2 门禁

| 项目 | PASS 证据要求 | 状态 | 证据 |
| --- | --- | --- | --- |
| Media/UI/OTA host checks | 可选组件 host CMake 测试和最小示例构建通过 | PASS | CI host-components job |
| Display | 板卡显示初始化、渲染一帧、输入事件回调日志和照片/视频 | PENDING | board.yaml 当前未验证 display |
| Audio | 播放提示音和采集一段音频，保存串口日志与音频文件 | PENDING | board.yaml 当前未验证 audio |
| Camera | 采集视频帧并完成一次局域网传输，保存帧统计、客户端日志和视频 | PENDING | board.yaml 当前未验证 camera |
| OTA | 下载、校验、应用、重启回滚路径的版本日志和镜像 SHA | PENDING | 待 SDK/硬件记录 |
| 外设 | 每个实际使用外设均有型号、引脚、读写日志和照片 | PENDING | 仅 cp_uart0 在 board.yaml 标记 verified |

## M3 门禁

| 项目 | PASS 证据要求 | 状态 | 证据 |
| --- | --- | --- | --- |
| Robot reference host | 无插件和可选插件 host 测试均通过 | PASS | CI robot-host job |
| 组合启动 | 网络、媒体、UI、OTA、外设生命周期日志无未处理错误 | PENDING | 待目标固件日志 |
| Optional Agora/AI | 明确记录外部 SDK ref、许可证审批、配置和端到端会话日志 | PENDING | 可选插件未执行 |
| Release artifact | SDK 原生 AP/CP 构建、`all-app.bin` SHA256 和配置快照 | PENDING | 待锁定 SDK 构建 |

## 独立硬件发布门禁

硬件门禁与普通 CI 分开执行。当前以下项目均为 `PENDING`，必须由 `bk7259_robot` 实机完成并附证据后才可改为 `PASS`：

- Boot：冷启动和复位各一次，串口日志包含版本、板卡和应用 ready。
- Provisioning：首次配网、断网重试、重复配网和重启恢复。
- LAN discovery：至少一个真实 LAN client 完成发现、命令和状态回传。
- Display：显示初始化、首帧、持续渲染和输入事件。
- Audio：播放、采集、停止和重复启动恢复。
- Camera：初始化、帧采集、传输和停止恢复。
- OTA：下载校验、应用、重启、失败回滚和版本确认。
- 已验证外设：逐项记录 `board.yaml` 中的能力、引脚、操作日志和实物照片。当前唯一 verified 能力是 `cp_uart0`，其他 audio、camera、display、input、led、vibration、tof、nfc 均待验证。

## 证据记录模板

每个 `PASS` 条目填写 `日期时间、固件/配置 SHA、硬件序列号、命令或日志路径、照片/视频路径、执行人`。未执行项保持 `PENDING`，不能用 host 构建结果替代硬件证据。
