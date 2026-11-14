# BK7259 USB 4G ECM 示例与 SDK Patch 集成设计

## 1. 目标

在 `bk-open-framework` 开源框架中新增 BK7259 QFN128B EVB 的 `usb_4g_ecm` 示例，同时保持框架仓库不携带 `bk_avdk_smp` SDK 源码。

构建时由框架仓库取得一个明确版本的 SDK，检查 SDK 基线，然后将框架仓库中的 USB ECM/MUSB/LwIP patch 应用到临时 SDK 工作区，最后使用该临时 SDK 编译固件。

目标硬件为：

- BK7259 QFN128B EVB V1.0；
- USB-A_FEMALE 作为 USB Host 口；
- GPIO32/P32（原理图信号 `UVC_ON`）控制 USB-A VBUS；
- 4G 模组使用 CDC-ECM 网卡和 vendor AT 接口；
- 当前目标模组 VID/PID 为 `3505:0003`，AT interface protocol 为 `0x03`。

## 2. 非目标

第一阶段不实现：

- PPP/RNDIS 动态切换；
- 多模组通用适配；
- Wi-Fi/4G 自动选路；
- NAT 转发；
- 低功耗；
- 4G 音频通话；
- OTA 传输策略；
- UVC 摄像头与 4G 同时使用。

## 3. 设计原则

1. 方案仓库不复制 SDK 源码，不直接修改用户指定的 `SDK_DIR`。
2. SDK patch 必须绑定明确的 SDK commit、文件校验值和 patch 顺序。
3. USB 控制器修复留在 SDK patch 中，模组 AT 和业务逻辑留在方案组件中。
4. 板级差异统一放在 `boards/<board>/`，示例工程只声明目标板并消费板级配置。
5. 构建失败时必须在 patch 阶段失败，不能静默使用未打补丁的 SDK。
6. 生成的临时 SDK 工作区可删除，不能提交到方案仓库。
7. 默认只支持一个模组 profile；扩展其他模组使用 profile 接口，不复制整个应用。

## 4. 参考实现基线

本方案的参考实现为：

- 仓库：[songjw0820/bk_avdk_smp](https://github.com/songjw0820/bk_avdk_smp)；
- 分支：`feature/usb-4g-ecm-at`；
- 参考用途：提取已经验证的应用层实现、整理 SDK patch、对照验证行为；
- 使用规则：不把该分支的 SDK 源码复制进 `bk-open-framework`，也不把运行时构建固定到浮动分支。

参考实现中的内容按下表迁移：

| 参考实现内容 | 迁移目标 | 处理方式 |
|---|---|---|
| `projects/multimedia/usb_4g_ecm/ap/modem_at.c` | `components/modem_ecm/src/modem_at.c` | 提取为公共 AT 管理层 |
| `projects/multimedia/usb_4g_ecm/ap/usb_modem_at.c` | `components/modem_ecm/src/usb_modem_at.c` | 提取为公共 vendor AT 传输层 |
| `modem_ecm_cfg.c` | `components/modem_ecm/src/modem_ecm_cfg.c` + profile | 将 Lierda 命令与通用接口分离 |
| `modem_monitor.c` | `components/modem_ecm/src/modem_monitor.c` | 作为可选监控组件 |
| `net_check.c`、`speed_test.c` | `examples/usb_4g_ecm/` | 保留为示例工具，不进入核心组件 |
| AP 启动顺序 | `examples/usb_4g_ecm/app/ap_main.c` | 保留 `lwIP → AT → USB Host` 顺序 |
| SDK USB/LwIP/MUSB 改动 | `sdk_patches/bk_avdk_smp/patches/` | 重新整理为可应用 patch |
| `test/host` | `components/modem_ecm/test/host` | 迁移为公共组件回归测试 |

参考分支中的主要实现阶段包括：

```text
ed13e45da  add usb_4g_ecm project
33ce655a3  build CDC-ECM host behind CONFIG_USBH_CDC_ECM
b55a5d7f4  CDC-ECM no-string-descriptor and netif teardown
17c6542e9  URB identity, reconnect gate, 4-bulk FIFO layout
9459f23c2  AT receive dangling-pointer race fix
4ea08606b  AT transport disconnect race fix
2848a8b2d  short-packet pipe reuse fix
df4489cc7  ECM receive buffer and modem dial configuration
d42cdedea  PSRAM ECM buffer and USB watchdog handling
01b8349bd  ECM configuration documentation
ec03178d6  automated speed test tool
6302eb7bf  modem signal monitor
a66f74903  stability and host test coverage
```

迁移时必须记录实际采用的 SDK 基线 commit。参考分支的最新提交不能直接当作 SDK 版本锁定值，因为其中同时包含应用代码、测试代码、文档和 SDK 修改。

## 5. 方案比较与选择

### 方案 A：只改项目代码

项目内复制 CDC-ECM、lwIP glue 和 vendor class，完全不处理 MUSB 端口。

优点：不需要 patch SDK。

缺点：无法可靠提供四个 MPS-512 bulk endpoint，URB 取消和热插拔仍受原 SDK 限制，适合临时验证，不适合正式示例。

### 方案 B：构建时应用 SDK patch（选定）

方案仓库保存 patch 和 patch manifest，构建时从干净 SDK 创建临时工作区并应用 patch。

优点：方案仓库不带 SDK，修改可审查、可回滚、可绑定版本，适合开源交付。

缺点：构建流程比普通 `make` 多一个准备阶段，SDK 升级必须重新验证 patch。

### 方案 C：依赖外部 SDK fork

方案仓库不带 SDK，直接要求用户使用已经打过补丁的外部 SDK fork。

优点：构建简单。

缺点：用户容易拿错 SDK，patch 内容不在方案仓库中，复现和审计较差。

本设计选择方案 B。方案 C 作为 CI 或发布包的可选加速方式。

## 6. 仓库结构

```text
bk-open-framework/
├── boards/
│   └── bk7259_qfn128b_evb_v1/
│       ├── board.yaml
│       ├── README_CN.md
│       ├── defconfig
│       ├── usr_gpio_cfg.h
│       └── partitions/
│           ├── auto_partitions.csv
│           └── ram_regions.csv
├── components/
│   └── modem_ecm/
│       ├── CMakeLists.txt
│       ├── Kconfig
│       ├── include/
│       │   ├── modem_ecm.h
│       │   ├── modem_at.h
│       │   └── modem_profile.h
│       └── src/
│           ├── usb_modem_at.c
│           ├── modem_at.c
│           ├── modem_at_queue.c
│           ├── modem_profile.c
│           ├── modem_ecm_cfg.c
│           ├── modem_net_manager.c
│           └── modem_monitor.c
├── examples/
│   └── usb_4g_ecm/
│       ├── CMakeLists.txt
│       ├── Makefile
│       ├── README_CN.md
│       └── app/
│           ├── ap_main.c
│           └── cli.c
├── sdk_patches/
│   └── bk_avdk_smp/
│       ├── manifest.yaml
│       ├── README_CN.md
│       └── patches/
│           ├── 0001-usb-build-enable-cdc-ecm.patch
│           ├── 0002-usb-musb-urb-lifetime.patch
│           ├── 0003-usb-musb-4bulk-fifo.patch
│           ├── 0004-usb-cdc-ecm-lwip-teardown.patch
│           └── 0005-usb-cdc-ecm-no-string-descriptor.patch
├── tools/
│   └── sdk_patch/
│       ├── apply_sdk_patches.py
│       ├── prepare_sdk_workspace.sh
│       └── build_with_sdk_patch.sh
└── docs/
    └── usb-4g-ecm/
        ├── hardware.md
        ├── sdk-patches.md
        └── validation.md
```

## 7. SDK Patch 设计

### 6.1 Patch 0001：CDC-ECM 编译接入

修改 SDK：

```text
ap/components/bk_usb/CMakeLists.txt
ap/components/bk_usb/Kconfig
ap/include/components/cherryusb/usb_config.h
```

内容：

- 增加 `CONFIG_USBH_CDC_ECM`；
- 将 `usbh_cdc_ecm.c` 纳入 BK 构建；
- 将 lwIP glue 纳入构建；
- 保持默认配置为关闭，避免影响其他工程；
- 加入 `CONFIG_USB_HOST_FIFO_LAYOUT_4BULK` 配置项。

### 6.2 Patch 0002：MUSB URB 生命周期

修改：

```text
ap/components/bk_usb/CherryUSB/port/beken_musb/usb_hc_beken_musb.c
ap/components/bk_usb/CherryUSB/class/hub/usbh_hub.c
```

内容：

- 按实际 slot owner 取消 URB；
- 记录 endpoint 到 pipe 的映射；
- disconnect 时关闭 submit gate；
- 重新枚举前由 hub 重新打开 submit gate；
- 防止旧设备 URB 提交到新设备 pipe。

### 6.3 Patch 0003：4-bulk FIFO

为以下四个端点提供独立 MPS-512 slot：

```text
ECM bulk IN
ECM bulk OUT
AT bulk IN
AT bulk OUT
```

要求：

- `CONFIG_USB_HOST_FIFO_LAYOUT_4BULK=y` 时拒绝 1024 字节 bulk endpoint；
- 保留默认布局作为其他工程的默认行为；
- 在 patch README 中说明该布局不适合大等时端点，例如 UVC/UAC。

### 6.4 Patch 0004：ECM lwIP teardown

修改：

```text
ap/components/bk_usb/CherryUSB/platform/lwip/usbh_lwip.c
ap/components/bk_usb/CherryUSB/class/cdc/usbh_cdc_ecm.c
```

内容：

- 使用 `netifapi_*` 访问 netif 和 DHCP；
- ECM disconnect 时停止 DHCP、删除 netif、等待 RX 线程退出；
- `netif_add()`、`dhcp_start()`、线程创建失败时反向回滚；
- 收包缓存使用 PSRAM custom pbuf，避免 TCP 长窗口耗尽 SRAM pbuf pool。

### 6.5 Patch 0005：无字符串描述符和连接通知

内容：

- 设备没有 MAC 字符串描述符时生成 locally administered MAC；
- CDC-ECM connection notification 使用有限等待；
- 在通知丢失时仍能启动数据路径；
- 避免 USB 模组无字符串描述符导致枚举失败。

## 8. Patch Manifest

`manifest.yaml` 示例：

```yaml
sdk_repo: https://github.com/bekencorp/bk_avdk_smp.git
sdk_commit: 24cce68adccebec25a7c3bb22e0d1de00f0da5cb
patch_format: git-format-patch
patch_dir: patches
patches:
  - 0001-usb-build-enable-cdc-ecm.patch
  - 0002-usb-musb-urb-lifetime.patch
  - 0003-usb-musb-4bulk-fifo.patch
  - 0004-usb-cdc-ecm-lwip-teardown.patch
  - 0005-usb-cdc-ecm-no-string-descriptor.patch
```

实际提交前应将 `sdk_commit` 更新为经过构建和板测的锁定版本，不能使用浮动分支名。

可选加入 SHA-256：

```yaml
source_sha256: <clean-sdk-archive-sha256>
patch_sha256:
  0001-usb-build-enable-cdc-ecm.patch: <sha256>
```

## 9. 构建流程

### 8.1 输入

```text
SDK_DIR       原始、干净的 bk_avdk_smp 路径
EXAMPLE       usb_4g_ecm
BOARD         bk7259_qfn128b_evb_v1
TARGET        bk7259
PATCH_MANIFEST sdk_patches/bk_avdk_smp/manifest.yaml
```

### 8.2 准备临时 SDK

```text
检查 SDK_DIR 存在
    ↓
读取 SDK HEAD
    ↓
校验是否等于 manifest.sdk_commit
    ↓
检查 SDK 工作区无未提交修改
    ↓
创建临时目录 .build/sdk/bk_avdk_smp
    ↓
git archive 或 rsync 复制 SDK
    ↓
按 manifest 顺序 git apply --3way
    ↓
执行 patch 自检
```

### 8.3 构建

```bash
./tools/sdk_patch/build_with_sdk_patch.sh \
    --sdk-dir /path/to/bk_avdk_smp \
    --example usb_4g_ecm \
    --board bk7259_qfn128b_evb_v1 \
    --soc bk7259
```

脚本必须导出：

```bash
SDK_DIR=<temporary-patched-sdk>
```

然后调用现有项目构建入口。

### 8.4 构建失败条件

以下任一情况必须立即失败：

- SDK commit 不匹配；
- SDK 工作区有未提交修改；
- patch 无法应用；
- patch 应用后关键文件校验不一致；
- `CONFIG_USBH_CDC_ECM` 未生效；
- `CONFIG_USB_HOST_FIFO_LAYOUT_4BULK` 未生效；
- 构建使用了原始 SDK 路径而不是临时路径。

## 10. 新板工程配置

### 10.1 原理图固化记录

本设计使用的板级证据来自以下原理图，后续实施无需重复解析原始 PDF：

- 文件：`/Users/soong/Documents/work/ai_work/armino/bk7259/SCH-BK7259_QFN128B_12.3X12.3_EVB_V1.0.pdf`；
- 原理图标题：`BK7259_QFN128B`；
- 相关页：第 1 页 `BK7259_QFN128B_Main`，第 4 页 `BK7259_QFN128B_Camera`；
- 芯片封装：BK7259 QFN128B 12.3 x 12.3。

已确认的硬件连接：

| 原理图事实 | 软件结论 |
|---|---|
| BK7259 USB_DN 为芯片 Pin 91 | USB Host D- 使用芯片 USB_DN |
| BK7259 USB_DP 为芯片 Pin 90 | USB Host D+ 使用芯片 USB_DP |
| USB-A_FEMALE 位于 `UVC CAMERA` 区域 | USB-A 是外部下游设备连接口，可用于 4G Host |
| USB-A VBUS 经 Q2 `SI2301` 高边开关供电 | 不能把模组 VBUS 直接当作固定电源，必须控制开关 |
| Q2 栅极由 Q3 `MMBT3904` 下拉 | 控制信号有效时打开 USB-A VBUS |
| Q3 基极经 R109 接 `UVC_ON`，R108 下拉 | `UVC_ON` 是 USB-A VBUS 控制信号 |
| BK7259 Pin 26 标注 `P32/DVP_D0`，网络名为 `UVC_ON` | USB VBUS 控制 GPIO 为 GPIO32/P32 |
| Type-C 的 CC1/CC2 各有 5.1K 下拉 | Type-C 角色和 USB-A Host 角色不能混为同一板级接口 |
| USB-A 与 Type-C 的 USB_DP/USB_DN 汇合到同一 USB PHY | 两个连接器不能作为两个独立 USB 控制器同时工作 |

因此板级配置固定为：

```text
CONFIG_USB_VBAT_CONTROL_GPIO_ID=0x20
```

其中：

```text
0x20 = 十进制 32 = GPIO32 = P32 = UVC_ON
```

### 10.2 板级使用限制

1. 4G 模组应连接 USB-A_FEMALE，不应把 Type-C 默认当作 4G Host 口。
2. USB-A 原本用于 UVC 摄像头；切换到 4G 后，UVC 和 4G 不能同时使用。
3. `CONFIG_USB_HOST_FIFO_LAYOUT_4BULK=y` 与大等时端点 UVC 不是同一资源布局，4G 示例必须关闭 UVC。
4. GPIO32 不得被 JPEG/DVP/UVC 复用占用，启动时必须作为普通 GPIO 输出。
5. 原理图只证明了 VBUS 开关拓扑，不能替代电源完整性验证；必须实测 5V 电压、模组启动峰值电流和 Q2 温升。
6. 如果新板更改 USB VBUS 控制 GPIO、USB 连接器或电源开关，必须新增 board 目录，不能复用本板的 `defconfig` 和 `usr_gpio_cfg.h`。

### 10.3 BSP 资源清单

以下信息来自同一份 QFN128B EVB 原理图，用于后续 BSP 配置、外设冲突检查和板级 README，不需要再次打开 PDF：

| 原理图页 | 资源 | BSP 记录 |
|---|---|---|
| 第 1 页 Main | USB | USB_DP/USB_DN 接 BK7259 Pin 90/91；USB-A 和 Type-C 共用同一 PHY |
| 第 1 页 Main | P32 | `P32/DVP_D0` 网络名为 `UVC_ON`，作为 USB-A VBUS 控制脚 |
| 第 1 页 Main | 电源树 | `VBAT` 为板上主电源；芯片内部生成 `VDDH`、`VDDD`、`VDDIG` 等电源域 |
| 第 2 页 Misc | UART0 | `DL_UART0_TX/RX` 接下载/Flash UART 接口 |
| 第 2 页 Misc | UART5/UART1 | 引出 `5TX/1TX`、`5RX/1RX`，用于调试或外部串口 |
| 第 2 页 Misc | SWD | 引出 `SWCLK`、`SWDIO`，用于调试下载 |
| 第 2 页 Misc | QSPI0/FT Flash | 引出 `QSPI0_IO0..IO3`、`QSPI0_SCK`、`QSPI0_CS` |
| 第 2 页 Misc | SD Card | 引出 `SD0_D0..D3`、`SD0_CLK`、`SD0_CMD` |
| 第 2 页 Misc | eMMC | 引出 `EMMC_D0..D7`、`EMMC_CLK`、`EMMC_CMD`、`EMMC_RST` |
| 第 2 页 Misc | Ethernet | H6 引出 `ENET_MDC/MDIO`、`ENET_TXD0..3`、`ENET_RXD0..3`、`ENET_REF/GRCLK` 等 |
| 第 2 页 Misc | 音频 | M3 为外部 Audio PA 接口；M1/M2/M4 为麦克风接口，另有 DMIC 信号 |
| 第 3 页 LCD | LCD | RGB LCD 接口、MIPI DSI LCD 接口，含 LCD CS/SDI/SCL、背光和触摸相关信号 |
| 第 3 页 LCD | 触摸 | 触摸接口使用 I2C、INT、RST 等信号，具体复用由 LCD/触摸 BSP 决定 |
| 第 4 页 Camera | DVP | DVP 摄像头接口使用 DVP_D0..D7、PCLK、HSYNC、VSYNC、MCLK 等固定复用资源 |
| 第 4 页 Camera | MIPI Camera | MIPI 摄像头接口含 CSI_DN/DP、MCLK、I2C、PWDN、RSTN 以及 1.2V/1.8V/2.8V 供电 |
| 第 4 页 Camera | USB Camera | USB-A_FEMALE 标注为 UVC CAMERA，4G Host 使用时与 UVC 互斥 |
| 第 5 页 Power | 5V/VBAT | `+5V` 经 BUCK 产生 `VBAT`；USB-A VBUS 由独立高边开关控制 |
| 第 5 页 Power | 摄像头电源 | 提供 2.8V、1.8V、1.2V 等摄像头/LCD 相关电源 |
| 第 5 页 Power | 按键/ADC | 多个按键通过电阻网络汇总到 `P25/ADC1`，另有 WAKEUP KEY |

BSP 使用规则：

1. USB 4G 示例启用 USB Host 后，必须检查 `P32/UVC_ON`、DVP、UVC 和 JPEG 复用冲突。
2. USB 4G 示例不应同时启用 UVC Camera；如果需要摄像头，应建立另一种 FIFO 和 USB 角色配置。
3. UART0 下载口、SWD 和调试串口属于启动/维护资源，不能被 AT 业务默认占用。
4. SD、eMMC、QSPI、LCD、DVP、MIPI、Ethernet 的 GPIO 复用必须以 board 目录中的 `usr_gpio_cfg.h` 为唯一配置来源。
5. 电源树中的 1.2V/1.8V/2.8V 主要服务 LCD/摄像头，不能作为 4G 模组 5V VBUS 的替代电源。
6. 4G 模组的实际峰值电流、VBUS 压降、Q2 温升仍需要实测，原理图记录不能替代电源验收。

### 10.4 AP defconfig

```text
CONFIG_USB=y
CONFIG_USB_HOST=y
CONFIG_USB_HUB=y
CONFIG_USB_RISCV_BRIDGE=y
CONFIG_USBH_CDC_ECM=y
CONFIG_USB_HOST_FIFO_LAYOUT_4BULK=y
CONFIG_USB_VBAT_CONTROL_GPIO_ID=0x20
CONFIG_LWIP_TCPIP_THREAD_STACKSIZE=1024
CONFIG_LWIP_PBUF_POOL_SIZE=48
CONFIG_LWIP_TCP_WND=65535
CONFIG_USB_DEVICE=n
CONFIG_USBD_MSC=n
CONFIG_USBH_UVC=n
CONFIG_USB_CAMERA=n
```

### 9.2 GPIO

GPIO32 必须为普通输出，USB Host 打开时拉高，关闭时拉低。

### 9.3 分区

沿用 BK7259 8M/16M 的对应分区模板，根据实际 AP 镜像大小确认：

- primary AP app；
- primary CP app；
- OTA；
- usr_config；
- easyflash；
- PSRAM region。

不能直接假定机器人项目的大型多媒体分区适合该 Host 示例。

## 11. 应用接口

### 10.1 初始化

```c
bk_err_t modem_ecm_init(const struct modem_profile *profile);
bk_err_t modem_ecm_start(void);
bk_err_t modem_ecm_stop(void);
```

### 10.2 状态查询

```c
bool modem_ecm_is_online(void);
bk_err_t modem_ecm_get_ip(ip4_addr_t *ip);
bk_err_t modem_ecm_get_signal(struct modem_signal *signal);
```

### 10.3 AT 请求

```c
int modem_at_request(const char *cmd,
                     char *resp,
                     uint32_t resp_size,
                     uint32_t timeout_ms);
```

第一阶段可以保留同步 API；第二阶段增加请求队列和异步完成回调。

### 10.4 事件

```c
typedef enum {
    MODEM_EVENT_ENUMERATED,
    MODEM_EVENT_AT_READY,
    MODEM_EVENT_DHCP_BOUND,
    MODEM_EVENT_ONLINE,
    MODEM_EVENT_OFFLINE,
    MODEM_EVENT_RECOVERING,
} modem_event_t;
```

## 12. 启动时序

```text
bk_init
  ↓
net_ipv4stack_init
  ↓
modem_ecm_init(profile)
  ↓
注册状态回调和 CLI
  ↓
bk_usb_open(USB_HOST_MODE)
  ↓
USB 枚举
  ↓
AT 通道上线
  ↓
检查并配置 ECM/NAT/auto-dial
  ↓
ECM netif 创建
  ↓
DHCP
  ↓
DNS/TCP 检测
  ↓
MODEM_EVENT_ONLINE
```

## 13. 验证计划

### 12.1 静态验证

```bash
python3 tools/sdk_patch/apply_sdk_patches.py --check
```

检查：

- SDK commit；
- patch 顺序；
- 关键符号；
- 无意外 SDK 文件提交。

### 12.2 主机侧测试

必须通过：

```bash
make -C components/modem_ecm/test/host run
make -C components/modem_ecm/test/host run-threads
```

覆盖：

- 跨 USB 包响应；
- 多行响应；
- URC；
- ERROR；
- 超时；
- 迟到 OK；
- 重入；
- 断开取消；
- 请求队列。

### 12.3 编译验证

```bash
./tools/sdk_patch/build_with_sdk_patch.sh \
    --sdk-dir "$SDK_DIR" \
    --example usb_4g_ecm \
    --board bk7259_qfn128b_evb_v1 \
    --soc bk7259
```

验收：

- AP/CP 编译成功；
- 无新增 warning；
- 生成 `package/all-app.bin`；
- map 中存在 `cdc_ecm_class_info` 和 `modem_at_class_info`；
- map 中确认 4-bulk 配置生效。

### 12.4 板级验证

顺序：

1. USB-A VBUS 电压和电流；
2. 设备枚举；
3. AT 通道上线；
4. `AT`、`AT+CSQ`、`AT+CEREG?`；
5. ECM DHCP；
6. DNS；
7. 短 TCP/HTTP；
8. AT 与 TCP 并行；
9. 10 次拔插；
10. 模组软复位和重新枚举；
11. 30 分钟并行压力；
12. 2 小时长稳。

### 12.5 验收指标

```text
枚举成功率：10/10
AT+CSQ 连续成功：100/100
DHCP：每次启动成功
DNS/TCP：连续 10 次成功
拔插：10 次无死锁、无崩溃
AT/TCP 并行：30 分钟无响应串线
长稳：2 小时无内存持续增长
```

## 14. CI 设计

CI 至少包含：

```text
patch-manifest-check
patch-apply-check
host-at-tests
host-thread-tests
project-config-check
full-build
artifact-layout-check
```

CI 必须使用固定 SDK commit，不允许使用 SDK 的 HEAD 或浮动分支。

建议缓存：

```text
clean SDK archive
patched SDK workspace keyed by sdk_commit + patch hashes
```

## 15. 回滚方案

### 回滚应用功能

删除或禁用：

```text
CONFIG_USBH_CDC_ECM
CONFIG_USB_HOST_FIFO_LAYOUT_4BULK
usb_4g_ecm example with bk7259_qfn128b_evb_v1 board
```

其他工程仍使用原始 SDK 默认 USB 配置。

### 回滚 SDK patch

只需：

```bash
rm -rf .build/sdk/bk_avdk_smp
```

并使用未应用 patch 的 SDK 构建其他项目。

### Patch 升级

SDK 升级必须重新执行：

```text
更新 sdk_commit
重新生成或调整 patch
静态检查
主机测试
完整构建
板级枚举
热插拔和长稳
```

禁止直接在 SDK 工作区手工修复后继续交付。

## 16. 风险与对策

| 风险 | 对策 |
|---|---|
| SDK commit 漂移 | manifest 锁定 commit 并在构建前校验 |
| patch 应用顺序错误 | 使用编号 patch 和自动 apply 检查 |
| 用户本地 SDK 有修改 | 构建前拒绝 dirty workspace |
| 4-bulk 与 UVC 冲突 | 新板示例关闭 UVC，文档明确互斥 |
| 模组协议差异 | 引入 modem profile |
| 热插拔死锁 | SDK patch 加 URB/pipe 生命周期修复，板测 10 次 |
| VBUS 电源不足 | 先做示波器/电流测试，再做网络验证 |
| macOS Bash 3.2 不支持 `wait -n` | 构建脚本改为兼容轮询或要求新版 Bash |
| 误用未打补丁 SDK | 构建日志打印 patched SDK 路径和 patch hash |

## 17. 交付物

第一阶段交付：

- `boards/bk7259_qfn128b_evb_v1` 板级描述；
- `examples/usb_4g_ecm` 示例工程；
- `components/modem_ecm`；
- 5 个 SDK patch；
- patch manifest；
- SDK patch 应用脚本；
- 构建脚本；
- 主机侧测试；
- 板级硬件说明；
- 验证报告；
- CI 构建任务。

## 18. 完成定义

满足以下条件后，认为该方案完成：

1. 方案仓库没有提交 `bk_avdk_smp` SDK 源码。
2. 干净 SDK 能根据 manifest 自动应用全部 patch。
3. SDK commit 不匹配时构建立即失败。
4. 新板工程能生成 AP/CP 固件。
5. BK7259 QFN128B EVB 能从 USB-A Host 口枚举 4G 模组。
6. ECM 和 AT 两条通道可以并行运行。
7. DHCP、DNS、TCP 均成功。
8. 10 次拔插无崩溃和死锁。
9. AT 与 TCP 并行 30 分钟无响应串线。
10. 2 小时长稳测试没有持续内存增长。
11. 其他未启用 USB ECM 的工程不受 patch 配置影响。
