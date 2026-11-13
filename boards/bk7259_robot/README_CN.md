# bk7259_robot 板卡

当前适配层只编码已核对的板级事实。CP UART0 的 RX 使用 GPIO10，TX 使用 GPIO11。

双麦克风、扬声器、摄像头、LCD、按键、LED、振动马达、ToF、环境光、G-Sensor 和 NFC 仅有文档或代码声明，尚未完成硬件验证，因此不会作为公共 API 中的已启用能力，也不会提供确定 pin。

使用 `components/bk_board/include/bk_board.h` 查询板卡信息、能力和已验证 pin。缺失能力返回 `false`，未知 pin 返回 `BK_ERR_NOT_FOUND`，未初始化访问返回 `BK_ERR_NOT_INITIALIZED`。
