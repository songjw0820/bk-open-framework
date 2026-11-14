# bk_solution_ai 与第三方依赖审计清单（锁定版本：`release/v4.0.1.11`）

审计基线（已通过远端源码树静态核对）：

- **`bekencorp/bk_avdk_smp`**：`tag_object_sha = be5e89d4f6b2f6f444727cad5fd0908a8dc93499`，`peeled_commit_sha = 584b947920c414230f9909375f008e7d6a2c6c6b`，仓库根目录 `LICENSE` 为 **Apache-2.0**。
- **`bekencorp/bk_solution_ai`**：`tag_object_sha = 009c95182de07ae874e39e3d3c24994431bcec70`，`peeled_commit_sha = a1e865e156e8dc31dfb360219c33a6a6d908ea98`，仓库根目录 `LICENSE` 为 **Apache-2.0**（整个源码树不含任何 `.a` / `.so` 预编译库）。

---

## 1. 候选框架模块对应的上游文件逐项许可证审计

> **原则**：`bk-open-framework` 核心仓库统一采用 **Apache-2.0** 许可证，以自研封装层（调用公开 SDK 头文件 API）实现，不直接复制无许可证头或含专有 SDK 依赖的源码文件。

| 框架目标组件 | 参考的上游路径（`release/v4.0.1.11`） | 文件头/仓库许可证核对结果 | 再分发分类 | `bk-open-framework` 实施处置 |
|---|---|---|---|---|
| `bk_config_store` | `bk_solution_ai/components/bk_factory_config/bk_factory_config.{c,h}` | 文件头显式声明 `Apache License, Version 2.0 (Copyright 2023-2024 Beken)` | 可再分发（Apache-2.0） | 自研实现 `bk_config_store` schema/namespace API，底层通过 `bk_port` 对接 Flash/KV，不直接拷贝原文件 |
| `bk_net_prov` | `bk_solution_ai/components/bk_smart_config/src/core/bk_smart_config_core.c`、`include/bk_smart_config.h` | 文件头显式声明 `Apache License, Version 2.0 (Copyright 2020-2025 Beken)` | 可再分发（Apache-2.0） | 自研实现 `bk_net_prov` 状态机，仅提取配网事件语义，剥离对 `network_engine` / Agora 的耦合 |
| `bk_video_pipeline` | `bk_solution_ai/components/video_engine/video_engine.{c,h}`、`components/multimedia_device_service/src/app_camera.c` | `video_engine.c` 显式声明 `Apache-2.0 (Copyright 2025-2026 Beken)`；`app_camera.c` 为 `Copyright 2020-2021 Beken` + 仓库根 `Apache-2.0` | 可再分发（Apache-2.0） | 自研实现 `bk_video_pipeline` 生命周期封装，直接调用 `bk_avdk_smp` 的 `bk_isp_camera.h` / `bk_camera_isp_ctlr.h` |
| `bk_audio_pipeline` | `bk_solution_ai/components/audio_engine/audio_engine.{c,h}` | 无单文件 SPDX 头，受仓库根 `LICENSE (Apache-2.0)` 约束；内部含 `#include <network_engine.h>` 耦合 | 仅外置引用，不拷贝源码 | 不拷贝 `audio_engine.c`；`bk_audio_pipeline` 直接基于 `bk_avdk_smp` 公开的 `bk_player_service.h` / `audio_pipeline` 编写干净的 Apache-2.0 实现 |
| `bk_board_bk7259_robot` (AP/CP 模板) | `bk_avdk_smp/projects/app/cp/**`（minimal）；`bk_solution_ai/projects/beken_robot/cp/config/bk7259/usr_gpio_cfg.h`、`cp_main.c`、`src/robot_keepalive.c`（robot） | `usr_gpio_cfg.h` 显式声明 `Apache-2.0 (Copyright 2020-2025 Beken)`；其余受仓库根 `Apache-2.0` 约束 | 外置受控模板（Apache-2.0） | `minimal` 仅引用外置 `bk_avdk_smp/projects/app/cp`；`robot` 仅通过外置 manifest 引用 `bk_solution_ai/projects/beken_robot/cp`，核心仓库不内置 `bk_solution_ai` 源码 |
| `bk_ai_agent/protocol_xiaozhi` | 外部协议规范参考：`78/xiaozhi-esp32/docs/websocket.md` | 上游仓库为 **MIT License** | 协议洁净室实现 | 仅按公开 WebSocket wire-format 规范独立编写 C 实现，不复制 `xiaozhi-esp32` C++ 源码；在文档与 NOTICE 中保留 MIT 致谢 |

---

## 2. 专有二进制库与第三方 SDK 审计（禁止进入公共核心仓库）

经对 `bk_avdk_smp`（`584b9479`）与 `bk_solution_ai`（`a1e865e1`）全量文件树检索，所有第三方专有二进制静态库均位于 `bk_avdk_smp/ap/components/bk_thirdparty/`，并在 `bk_solution_ai/components/network_engine/CMakeLists.txt` 中通过 Kconfig 条件引用：

| 组件/库名称 | 真实仓库与路径（`release/v4.0.1.11`） | 启用宏（Kconfig） | 许可证性质 | 框架隔离策略 |
|---|---|---|---|---|
| Agora RTC SDK 静态库 | `bk_avdk_smp/ap/components/bk_thirdparty/agora-iot-sdk/bk7259/libs/libagora-rtc-sdk.a` | `CONFIG_AGORA_IOT_SDK` | 声网商业/专有二进制库（需 AppID/License 授权） | **严禁进入核心仓库与默认构建**；仅在 `bk7259_robot` 可选 `plugin_agora` 开启时由外置 SDK 链接 |
| Agora RTC/RTM 适配层 | `bk_solution_ai/components/network_engine/agora_rtc/{bk_agora_api.c,agora_rtc_engine.c,agora_agent_engine.c,bk_agora_rtm.c,agent_utils/AgoraWebClientUtils.c}` | `CONFIG_AGORA_IOT_SDK` | 依赖专有 `agora-iot-sdk` 头文件 | 归入可选插件范围，默认关闭，不进入 `bk_ai_agent` 公共 API |
| 火山引擎 RTC Lite 静态库 | `bk_avdk_smp/ap/components/bk_thirdparty/VolcEngineRTCLite/lib/libVolcEngineRTCLite.a` | `CONFIG_VOLC_RTC_EN` | 火山引擎专有二进制库 | **不接入核心仓库**；公共框架不依赖 `VolcEngineRTCLite` |
| 火山引擎适配层 | `bk_solution_ai/components/network_engine/volc_rtc/{bk_volc_api.c,volc_rtc_engine.c,volc_agent_engine.c,agent_utils/RtcWebClientUtils.c}` | `CONFIG_VOLC_RTC_EN` | 依赖 `VolcEngineRTCLite` | **不接入核心仓库** |

---

## 3. 模型权重、语音提示音、字体与测试密钥（禁止进入公共核心仓库）

以下位于 `bk_solution_ai`（`a1e865e1`）的非代码资产或生成资源缺少独立再分发授权声明或属于演示专用资产，**一律不复制、不打包进 `bk-open-framework`**：

| 资产类别 | 具体路径（`bk_solution_ai` @ `a1e865e1`） | 审计结论与处置方式 |
|---|---|---|
| TFLite / Vela 端侧视觉模型 | `components/avdk_nn_module/src/*/*.tflite`、`projects/beken_robot/resources/tflite_model/{face_detection_int8_vela,face_verify_int8_vela,hand_gesture_detection_vela,palm_detection_builtin_256_integer_quant_vela,yoloface_int8_vela}.tflite` 及对应 `*_model_data.cc` | 权重来源与训练集许可证未单独声明；仅保留在 `bk_solution_ai` 外置仓库，不进入 `bk-open-framework` |
| KWS 唤醒词与命令词模型 | `projects/{beken_robot,secureboot_ai}/resources/kws_model/{bk_kws_wakeup,bk_kws_commands}.tflite` | 专有唤醒词模型；仅保留在外置 `bk_solution_ai` 仓库 |
| MP3/PCM/WAV 语音提示音 | `projects/{beken_robot,secureboot_ai}/resources/*.mp3`（共 13 个英语提示音）、`components/bk_app_event/{prompt_tone_mp3_array,prompt_tone_pcm_array,prompt_tone_wav_array,test1_mp3_array}.h` | 音频资源版权未单独声明；`bk-open-framework` 不复制任何 `*_array.h` 或 `.mp3` |
| GUI 生成代码、图片与字体 | `projects/beken_robot/ap/beken_generated/**`（含 `fonts/`、`image/`、`page_*/`）、`projects/beken_robot/ap/include/lcd_image.h`、`docs/bk7259/_static/DejaVuSans.ttf` | `beken_robot` 演示 UI 专用资产；不进入 `bk-open-framework` |
| 安全启动演示密钥对 | `projects/secureboot_ai/config/key/{root_ec256_privkey.pem,root_ec256_pubkey.pem}` | 公开演示测试私钥（**高危测试资产**）；严禁在框架中复用，框架仅支持注入外部密钥路径 |
