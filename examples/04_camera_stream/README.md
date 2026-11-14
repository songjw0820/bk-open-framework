# Example 04: Camera Frame Pool & LVGL UI Synchronization

Demonstrates 64-byte aligned PSRAM video frame acquisition, atomic retain/release, explicit D-Cache synchronization (`bk_frame_sync_for_cpu` / `bk_frame_sync_for_device`), and recursive `bk_ui_lock`.
