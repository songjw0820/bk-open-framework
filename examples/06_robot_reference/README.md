# Example 06: Open-Source AI Robot Reference (`bk7259_robot`)

Demonstrates full-stack orchestration of `bk_ai_agent` (`transport_websocket` + `protocol_xiaozhi` MCP JSON-RPC 2.0 primary path), `bk_audio_session`, `bk_ui_core`, `bk_motion`, `bk_robot_face`, and `bk_battery_policy` in pure open-source mode **without requiring the proprietary Agora SDK (`plugin_agora`)**.

## Verification Scope

- **Tier 1 (`posix_sim` Host)**: Runs a real local loopback TCP/WebSocket server (`ws://127.0.0.1:<port>/xiaozhi/v1/`), verifies RFC 6455 HTTP/1.1 101 Upgrade + `Sec-WebSocket-Accept` + Xiaozhi headers (`Authorization`, `Protocol-Version`, `Device-Id`, `Client-Id`), exchanges `hello` and MCP JSON-RPC 2.0 tool calls, and encodes/decodes 60ms Opus audio frames via native `libopus`.
- **Tier 2 / Tier 4 (`bk7259_v4` Target & Hardware — Pending)**: Remote cloud `wss://` TLS connection and physical microphone/speaker/servo interaction on `bk7259_robot` hardware remain gated by the Tier 2/4 checklist in [`docs/release-checklist.md`](../../docs/release-checklist.md).
