# 乐鑫组件注册表（Espressif Component Registry）经典组件移植与兼容指南

- **目标生态**：[https://components.espressif.com/](https://components.espressif.com/)（ESP-IDF / ESP-IoT-Solution / ESP-BSP / ESP-ADF / 社区组件）
- **兼容层组件**：[`components/bk_esp_compat`](../components/bk_esp_compat/)
- **CMake 构建桥接**：[`components/bk_esp_compat/cmake/bk_esp_idf_compat.cmake`](../components/bk_esp_compat/cmake/bk_esp_idf_compat.cmake)

---

## 1. 核心结论：乐鑫经典组件能否直接移植到本框架？

**建议按架构耦合度分为 4 个层级（Tier A ~ Tier D）。Tier A 和部分 Tier B 可以直接复用，或通过 [`bk_esp_compat`](../components/bk_esp_compat/) 适配。Tier C 通常需要总线 HAL 适配。Tier D 需要使用 BK7259 Armino 原生能力重写或替换。是否零修改必须以具体组件版本和目标构建结果为准。**

| 分级 | 典型代表组件（来自 `components.espressif.com`） | 移植方式 | 源码改动量 |
| :--- | :--- | :--- | :--- |
| **Tier A：纯 C/C++ 算法、协议与 GUI 库** | `cjson`、`lvgl/lvgl`、`espressif/ThorVG`、`lvgl/lv_lib_png`、`espressif/zlib`、`espressif/qrcode`、`78/esp-ml307`（解析层） | 适配构建注册和依赖后进行 Host 与目标构建验证 | **以组件版本为准** |
| **Tier B：仅依赖 ESP-IDF 系统原语与 `esp-iot-solution` 交互组件** | `espressif/button` (`iot_button`)、`espressif/led_indicator`、`espressif/cmake_utilities`、基于 `esp_timer` / `esp_event` / `nvs` / `esp_heap_caps` / `esp_log` 的业务状态机 | 链接 [`bk_esp_compat`](../components/bk_esp_compat/)，头文件与 API 直接桥接到 `bk_port`、`bk_core_event`、`bk_storage`、`bk_button`、`bk_led_effect` | **0 ~ 10 行改动** |
| **Tier C：I2C/SPI/UART 传感器、触控与外设芯片驱动** | `espressif/mpu6050`、`espressif/icm42670`、`espressif/bme280`、`espressif/sht3x`、`espressif/esp_lcd_touch_ft5x06`、`espressif/esp_lcd_ili9341`、`espressif/esp_codec_dev` | 保留全部寄存器配置与补偿算法，将底层 `i2c_master_transmit_receive` / `spi_device_polling_transmit` 适配到 Armino `bk_i2c_*` / `bk_spi_*` | **50 ~ 150 行总线 Adapter** |
| **Tier D：紧耦合 Xtensa/RISC-V SIMD 指令集或 ESP32 专有硬件外设** | `espressif/esp-sr`（WakeNet/MultiNet 闭源二进制）、`espressif/esp-dl`、`espressif/esp-dsp`（PIE 汇编部）、`espressif/esp32-camera`、`espressif/esp_wifi` | **不可直接移植源码/二进制**；在本框架中通过 `bk_audio_session`、`bk_video_frame_pool`、`bk_net` 对接 BK7259 Arm Cortex-M55 Helium/Ethos-U 及 CP 核硬件能力 | **使用本框架原生组件替代** |

---

## 2. 本框架内置的 `bk_esp_compat` 兼容层架构

为了消除从 ESP-IDF 移植组件时的样板改造工作，本框架在 [`components/bk_esp_compat`](../components/bk_esp_compat/) 中实现了构建期与运行期双层桥接：

### 2.1 构建期桥接：`idf_component_register` -> `armino_component_register`

乐鑫组件仓库中的所有组件均在 `CMakeLists.txt` 中使用 `idf_component_register(SRCS ... INCLUDE_DIRS ... REQUIRES ...)`。
通过包含 [`components/bk_esp_compat/cmake/bk_esp_idf_compat.cmake`](../components/bk_esp_compat/cmake/bk_esp_idf_compat.cmake)：

- 自动解析当前桥接器支持的 `SRCS`、`SRC_DIRS`、`EXCLUDE_SRCS`、`INCLUDE_DIRS`、`PRIV_INCLUDE_DIRS`、`REQUIRES`、`PRIV_REQUIRES`。
- 将已列入映射表的 ESP-IDF 依赖名重定向到 `bk_esp_compat`。未列入映射表的依赖仍需要目标工程提供。
- 在 Beken Armino SMP 工具链环境下自动调用 `armino_component_register()`；在 Host `posix_sim` 环境下自动生成标准 CMake 静态库目标并注入 `-include sdkconfig.h`。

### 2.2 运行期头文件与原语桥接矩阵

| ESP-IDF / ESP-IoT-Solution 头文件 | 本框架兼容实现路径 | 底层映射的 BK Open Framework 组件与契约 |
| :--- | :--- | :--- |
| `esp_err.h` / `esp_check.h` | [`components/bk_esp_compat/include/esp_err.h`](../components/bk_esp_compat/include/esp_err.h)、[`esp_check.h`](../components/bk_esp_compat/include/esp_check.h) | 将 `ESP_OK` / `ESP_ERR_NO_MEM` / `ESP_ERR_INVALID_ARG` / `ESP_ERR_INVALID_STATE` / `ESP_ERR_TIMEOUT` 双向映射到 `bk_err_t`（`bk_err_to_esp()` / `esp_err_to_bk()`），完整支持 `ESP_RETURN_ON_ERROR` 与 `ESP_GOTO_ON_ERROR` |
| `esp_log.h` | [`components/bk_esp_compat/include/esp_log.h`](../components/bk_esp_compat/include/esp_log.h) | 映射 `ESP_LOGE/W/I/D/V` 到 `BK_LOGE/W/I/D` |
| `esp_heap_caps.h` | [`components/bk_esp_compat/include/esp_heap_caps.h`](../components/bk_esp_compat/include/esp_heap_caps.h) | `MALLOC_CAP_SPIRAM` 映射到 BK7259 PSRAM 分配路径；`MALLOC_CAP_DMA` 映射到 `BK_MEM_CAP_DMA_ALIGN64`（请求 64B 对齐）；`MALLOC_CAP_INTERNAL` 映射到内部 SRAM 路径。实际外设 DMA 可访问性仍需板级验证 |
| `esp_timer.h` | [`components/bk_esp_compat/include/esp_timer.h`](../components/bk_esp_compat/include/esp_timer.h) | `esp_timer_get_time()` 返回微秒单调时钟。当前计时器由 `bk_esp_timer_step_us()` 驱动，目标平台后台调度和并发语义仍需验证 |
| `esp_event.h` | [`components/bk_esp_compat/include/esp_event.h`](../components/bk_esp_compat/include/esp_event.h) | 已实现部分 event loop、注册和 post API，并映射到 [`bk_core_event`](../components/bk_core_event/include/bk_core_event.h)。完整 ESP-IDF 事件 API 兼容性仍需验证 |
| `nvs.h` / `nvs_flash.h` | [`components/bk_esp_compat/include/nvs.h`](../components/bk_esp_compat/include/nvs.h) | 已实现常用 blob、整数、字符串读写和命名空间操作，并映射到 [`bk_storage`](../components/bk_storage/include/bk_storage.h)。`nvs_commit` 的事务语义与真实 NVS 仍需验证 |
| `iot_button.h` (`espressif/button`) | [`components/bk_esp_compat/include/iot_button.h`](../components/bk_esp_compat/include/iot_button.h) | 已覆盖当前示例使用的 GPIO、ADC 和常用按键回调，底层由 [`bk_button`](../components/bk_button/include/bk_button.h) 状态机驱动 |
| `led_indicator.h` (`espressif/led_indicator`) | [`components/bk_esp_compat/include/led_indicator.h`](../components/bk_esp_compat/include/led_indicator.h) | 已覆盖当前适配层定义的创建、启动和停止接口，底层由 [`bk_led_effect`](../components/bk_led_effect/include/bk_led_effect.h) 驱动。具体 Registry 版本 API 需要逐项对照 |

---

## 3. 实战移植步骤（Step-by-Step）

### 场景 1：直接复用 `espressif/button` 或 `espressif/led_indicator` 业务代码

无需下载外部源码，直接包含兼容层头文件即可运行原本为 ESP32 编写的交互代码：

```c
#include "iot_button.h"
#include "led_indicator.h"

static void on_double_click(void *button_handle, void *usr_data) {
    led_indicator_handle_t led = (led_indicator_handle_t)usr_data;
    led_indicator_start(led, BLINK_FAST);
}

void app_input_init(void) {
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 1500,
        .short_press_time = 180,
        .gpio_button_config = { .gpio_num = 12, .active_level = 0 },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, on_double_click, my_led);
}
```

### 场景 2：从 `https://components.espressif.com/` 引入第三方组件（以传感器或协议库为例）

1. 将下载的组件放入 `components/<vendor>_<name>/`。
2. 在该组件的 `CMakeLists.txt` 顶部加入一行：
   ```cmake
   include(${CMAKE_SOURCE_DIR}/components/bk_esp_compat/cmake/bk_esp_idf_compat.cmake)
   idf_component_register(
       SRCS "foo.c"
       INCLUDE_DIRS "include"
       REQUIRES esp_timer nvs_flash log
   )
   ```
3. 若组件为 Tier C（含 I2C/SPI 读写函数），将其 `i2c_bus_read_bytes()` / `i2c_bus_write_bytes()` 回调指向 `bk_board` 提供的已验证总线句柄。
4. 先在 Host `posix_sim` 下编写 CTest 单元测试（开启 ASan/UBSan），通过后再放入 `bk7259_robot` 工程交叉编译。

---

## 4. 移植到 BK7259 时的三大架构红线（避坑指南）

1. **DMA 与 D-Cache 对齐差异（32B/64B vs ESP32）**：
   - ESP32-S3 支持非对齐或 16/32B 对齐的内部/外部内存 DMA；而 **BK7259 的 `bk_frame_queue` 与 Cache API 强制要求 64 字节对齐（`BK_FRAME_QUEUE_ALIGN_SIZE = 64`）**。
   - 使用 `heap_caps_malloc(..., MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM)` 的组件会请求 64B 对齐，但外设能否直接访问对应 PSRAM 区域仍需按板级实测确认，不能只依据分配成功判断 DMA 可用。
2. **双核 SMP/AMP 职责边界（AP Cortex-M55 vs CP Cortex-M52）**：
   - 在 ESP32 上，Wi-Fi/BLE Controller 与应用任务运行在同一套 FreeRTOS SMP 内核；而在 BK7259 上，底层射频、音频硬件流水线或部分协处理任务可能分布在 AP 与 CP 两颗独立核心上，跨核缓冲区严禁直接传递栈指针。
3. **声学前端（AFE / WakeNet）替换原则**：
   - 乐鑫的 `esp-sr` 与 ESP32 专有运行时和二进制依赖耦合，不能直接假定可移植到 BK7259。在 BK7259 上应使用本框架的 [`bk_audio_session`](../components/bk_audio_session/include/bk_audio_session.h) 对接已经验证的 Armino AEC/VAD/KWS 算法链。
