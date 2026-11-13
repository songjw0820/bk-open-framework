# Task 4 报告：bk_board 与 bk7259_robot

## 实现内容

- 新增 `bk_board` 公共 C API、host standalone CMake、Kconfig 和能力测试。
- 新增 `bk7259_robot` 板卡清单、pin 头文件和中文说明。
- 公共能力只启用已核对的 CP UART0。
- CP UART0 RX/TX 分别编码为 GPIO10/GPIO11。
- 音频、摄像头、显示、输入、LED、振动、ToF、NFC 等保持未启用，未知 pin 返回 `BK_ERR_NOT_FOUND`。
- API 不依赖 BK7259 芯片宏。

## 测试命令与输出

命令：

```text
cmake -S components/bk_board -B /tmp/bk_board_build2 -DBK_BOARD_BUILD_TESTS=ON
cmake --build /tmp/bk_board_build2
ctest --test-dir /tmp/bk_board_build2 --output-on-failure
```

结果：

```text
[100%] Built target bk_board_test
1/1 Test #1: bk_board_test ....................   Passed    0.47 sec
100% tests passed, 0 tests failed out of 1
```

覆盖了未初始化访问、空参数、错误板名、重复 init、已验证 UART 能力、未验证能力、GPIO10/11、未知 pin 和空 pin 输出参数。

## 验证边界

未执行锁定 SDK 的 Armino compile smoke、AP/CP 构建、镜像打包或硬件测试。当前 pin 和能力仅依据 Task 0 已验证事实。其他板级器件不作为 enabled capability 或确定 pin。

## Commit

实现提交由本任务提交后填入 SHA。

## Concerns

`bk_board` 当前没有 deinit API，host 测试通过单进程生命周期覆盖重复 init 和状态边界。若后续需要多次独立板卡初始化，应增加显式 deinit 或测试进程隔离。
