# bk_ui

`bk_ui` 提供芯片无关的显示渲染、输入事件和 UI 生命周期接口。显示驱动或 SDK UI 框架通过 `bk_ui_adapter_t` 的 opaque context 与回调注入。

未注入 adapter 时，初始化返回 `BK_ERR_NOT_FOUND`。已初始化但缺少具体操作回调时，操作返回 `BK_ERR_NOT_SUPPORTED`。该组件不包含 Agora、机器人业务或芯片宏。
