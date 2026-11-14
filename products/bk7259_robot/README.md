# Product Reference: `bk7259_robot`

- **Profile Manifest**: [`platforms/bk7259_robot.yaml`](../../platforms/bk7259_robot.yaml)
- **Board Capability Table**: [`boards/bk7259_robot/board.yaml`](../../boards/bk7259_robot/board.yaml)
- **Candidate Re-budgeted OTA Partition CSV**: [`boards/bk7259_robot/partitions/auto_partitions_ota.csv`](../../boards/bk7259_robot/partitions/auto_partitions_ota.csv)
- **Product Controller**: [`include/bk_robot_product.h`](include/bk_robot_product.h) / [`src/bk_robot_product.c`](src/bk_robot_product.c)
- **AP Defconfig Fragment**: [`ap/config/bk7259_ap/defconfig.fragment`](ap/config/bk7259_ap/defconfig.fragment)
- **Controlled CP Template**: `bekencorp/bk_solution_ai` (`projects/beken_robot/cp` at `release/v4.0.1.11`, commit `a1e865e156e8dc31dfb360219c33a6a6d908ea98`)
- **Primary AI Protocol**: `protocol_xiaozhi` (RFC 6455 WebSocket + 3×20ms->60ms Opus + MCP JSON-RPC 2.0 + `listen`/`abort`/`tts`/`llm`/`stt`)
- **Optional Proprietary Plugin**: `plugin_agora` (`CONFIG_BK_PLUGIN_AGORA_ENABLE=n` by default)
- **Deferred Capability**: `bk_ota` (implemented in [`components/bk_ota`](../../components/bk_ota) and verified in `test_ota_fsm`; kept in `deferred_target_components` until the 5 OTA partition/size prerequisites in [`docs/release-checklist.md`](../../docs/release-checklist.md) are measured on target hardware)
