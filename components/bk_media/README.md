# bk_media

`bk_media` 提供芯片无关的音视频生命周期和数据提交接口。SDK 编解码器、音频设备或视频管线通过 `bk_media_adapter_t` 的 opaque context 与回调注入。

未注入 adapter 时，初始化返回 `BK_ERR_NOT_FOUND`。已初始化但缺少具体操作回调时，操作返回 `BK_ERR_NOT_SUPPORTED`。该组件不包含 Agora、机器人业务或芯片宏。

Standalone CMake 默认关闭组件。使用 `-DCONFIG_BK_MEDIA=ON` 才会生成 `bk_media` target，测试可再加 `-DBK_MEDIA_BUILD_TESTS=ON`。
