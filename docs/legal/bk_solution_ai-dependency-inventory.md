# bk_solution_ai 依赖清单（Task 0）

范围是为框架迁移建立逐项审计入口。未完成许可证核对的文件不进入公共框架仓库。

| 项目 | 来源路径 | 处理方式 | 许可证/再分发状态 |
|---|---|---|---|
| Armino SDK API 和底层库 | 外部 `bk_avdk_smp` | 通过 manifest 外置依赖 | 由 SDK 仓库负责，待随版本核对 |
| 网络、事件、媒体和板卡代码 | `components/*`、`projects/beken_robot/ap`、`cp` | 逐文件抽取适配 | 当前未完成逐文件审计 |
| Agora RTC/RTSA/RTM | `README_CN.md`、`projects/beken_robot/ap` | robot profile 可选插件 | 专有依赖，不进入核心仓库 |
| AI Agent 云端接口 | `projects/beken_robot/ap` 和 README | 保留插件接口 | 专有服务，需单独授权 |
| OPUS/H.264/JPEG 能力 | README、媒体组件和 SDK 接口 | 复用 SDK 或插件 | 逐项确认 SDK/第三方许可 |
| 音频提示音和 PCM/MP3 资源 | `components/bk_app_event` | 暂不抽取 | 资源来源和再分发权待核对 |
| NPU 模型和生成资源 | `projects/beken_robot/ap/beken_generated` 及相关资源 | robot profile 专用 | 模型、字体和生成资源待核对 |
| 字体、图片和 LCD 资源 | `projects/beken_robot/ap/beken_generated`、`ap/include` | robot profile 专用 | 逐项核对字体和图片许可 |
| 第三方二进制库 | 工程构建配置和链接参数 | 只通过插件引用 | 待列出文件名、版本和 NOTICE |

## 已发现的第三方或专有引用

| 引用 | 位置 | 当前处理 |
|---|---|---|
| Agora RTC/RTSA/RTM API | `projects/beken_robot/README.md`、`README_CN.md`、`ap/src/demo/camera_preview.c`、`components/bk_app_event/app_event.c` | 记录为 robot profile 可选插件，具体 SDK 文件和版本待核对 |
| `agora-iot-sdk` 内存说明 | `projects/beken_robot/partitions/bk7259/ram_regions.csv` | 仅作为来源注释，未复制，许可证和版本待核对 |
| 生成字体资源 | `projects/beken_robot/ap/beken_generated/fonts/*.c` | robot profile 专用，字体来源和再分发权待核对 |
| 提示音和媒体资源 | `components/bk_app_event/*_array.h`、`projects/beken_robot/ap/include/lcd_image.h` | robot profile 专用，资源来源和再分发权待核对 |
| NPU/端侧模型代码和资源 | `components/avdk_nn_module/`、`projects/beken_robot/ap/beken_generated/` | robot profile 专用，模型许可证和权重来源待核对 |
| 示例安全启动 EC256 私钥 | `projects/secureboot_ai/config/key/root_ec256_privkey.pem`、`projects/secureboot_headset_lp/config/key/root_ec256_privkey.pem` | 公开仓库测试密钥，禁止复制到框架仓库，量产必须外部注入独立密钥 |

当前本地 checkout 没有可确认的 Agora 二进制文件名、版本号或 NOTICE 文件，因此这些字段保留为待核对状态，不把类别名称当成已完成的法律审计。

## 已核对的审计入口

- `LICENSE`、`docs/`：仓库级许可证和说明。
- `projects/beken_robot/Makefile`、`ap/CMakeLists.txt`、`cp/CMakeLists.txt`：构建和链接入口。
- `components/*/Kconfig`、`components/*/CMakeLists.txt`：组件依赖和配置入口。
- `README.md`、`README_CN.md`：Agora、AI Agent、音视频和板卡能力声明。
