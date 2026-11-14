# Product Reference: `bk7259_robot`

- **Profile Manifest**: [`platforms/bk7259_robot.yaml`](../../platforms/bk7259_robot.yaml)
- **Board Capability Table**: [`boards/bk7259_robot/board.yaml`](../../boards/bk7259_robot/board.yaml)
- **Controlled CP Template**: `bekencorp/bk_solution_ai` (`projects/beken_robot/cp` at `release/v4.0.1.11`, commit `a1e865e156e8dc31dfb360219c33a6a6d908ea98`)
- **Primary AI Protocol**: `protocol_xiaozhi` (WebSocket + 3×20ms->60ms Opus + MCP JSON-RPC 2.0)
- **Optional Proprietary Plugin**: `plugin_agora` (`CONFIG_BK_PLUGIN_AGORA_ENABLE=n` by default)
- **Deferred Capability**: `bk_ota` (deferred until 5 OTA partition/size prerequisites in `docs/release-checklist.md` are verified)
