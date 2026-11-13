"""Check example metadata and host entry points without a vendor SDK."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.manifest import load_manifest

KNOWN_PROFILES = {"minimal", "robot"}
KNOWN_COMPONENTS = {
    "bk_core", "bk_board", "bk_net", "bk_media", "bk_ui",
    "bk_ota", "bk_peripheral",
}
MINIMAL_COMPONENTS = {"bk_core", "bk_board", "bk_net"}
MINIMAL_FORBIDDEN_TOKENS = (
    "bk_solution_ai",
    "bk_media",
    "bk_ui",
    "bk_ota",
    "bk_peripheral",
    "agora",
)
EXPECTED = [
    "01_wifi_provision",
    "02_lan_device",
    "03_audio_player",
    "04_camera_stream",
    "05_peripheral_control",
    "06_robot_reference",
    "07_ui_control",
    "08_ota_update",
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
    profile = data.get("profile")
    if profile not in KNOWN_PROFILES:
        errors.append(f"{name}: unsupported profile: {profile}")
    components = data.get("components")
    if not isinstance(components, list) or not components or any(
        not isinstance(component, str) or component not in KNOWN_COMPONENTS
        for component in components
    ):
        errors.append(f"{name}: components must be a non-empty list of known components")
    manifest_ref = data.get("platform_manifest")
    if not isinstance(manifest_ref, str) or not manifest_ref:
        return errors
    platform_manifest = path / manifest_ref
    if not platform_manifest.is_file():
        errors.append(f"{name}: platform_manifest does not exist: {data.get('platform_manifest')}")
        return errors
    try:
        platform = load_manifest(platform_manifest)
    except (OSError, ValueError) as exc:
        errors.append(f"{name}: invalid platform manifest: {exc}")
        return errors
    if data.get("platform") != platform.get("chip"):
        errors.append(f"{name}: platform must match platform manifest chip")
    if profile != platform.get("profile"):
        errors.append(f"{name}: profile must match platform manifest profile")
    if data.get("board") != platform.get("board"):
        errors.append(f"{name}: board must match platform manifest board")
    platform_components = platform.get("components") or []
    if isinstance(components, list) and any(component not in platform_components for component in components):
        errors.append(f"{name}: components must be enabled by the selected platform profile")
    if profile == "minimal":
        unexpected = sorted(set(components) - MINIMAL_COMPONENTS) if isinstance(components, list) else []
        if unexpected:
            errors.append(f"{name}: minimal examples cannot enable components: {', '.join(unexpected)}")
        for source_name in ("example.yaml", "README_CN.md", "CMakeLists.txt", "main.c"):
            source = path / source_name
            text = source.read_text(encoding="utf-8").lower()
            found = [token for token in MINIMAL_FORBIDDEN_TOKENS if token in text]
            if found:
                errors.append(
                    f"{name}: minimal example references forbidden dependencies in {source_name}: {', '.join(found)}"
                )
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
