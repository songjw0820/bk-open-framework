"""Check example metadata and host entry points without a vendor SDK."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.manifest import load_manifest
EXPECTED = [
    "01_wifi_provision",
    "02_lan_device",
    "03_audio_player",
    "04_camera_stream",
    "05_peripheral_control",
    "06_robot_reference",
]


def check_example(name: str) -> list[str]:
    path = ROOT / "examples" / name
    errors: list[str] = []
    for required in ("example.yaml", "README_CN.md", "CMakeLists.txt", "main.c"):
        if not (path / required).is_file():
            errors.append(f"{name}: missing {required}")
    if errors:
        return errors
    try:
        data = load_manifest(path / "example.yaml")
    except (OSError, ValueError) as exc:
        return [f"{name}: invalid example.yaml: {exc}"]
    for key in ("name", "milestone", "platform", "platform_manifest", "profile", "components"):
        if key not in data or data[key] in (None, "", []):
            errors.append(f"{name}: missing {key}")
    if data.get("name") != name:
        errors.append(f"{name}: name must match directory")
    if data.get("milestone") not in {"M1", "M2", "M3"}:
        errors.append(f"{name}: milestone must be M1, M2 or M3")
    if not isinstance(data.get("components"), list):
        errors.append(f"{name}: components must be a list")
    platform_manifest = path / data.get("platform_manifest", "")
    if not platform_manifest.is_file():
        errors.append(f"{name}: platform_manifest does not exist: {data.get('platform_manifest')}")
    return errors


def main() -> int:
    errors = [error for name in EXPECTED for error in check_example(name)]
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    for name in EXPECTED:
        print(f"valid: examples/{name}/example.yaml")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
