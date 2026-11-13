"""Profile and dependency helpers for the BK framework CLI."""
from __future__ import annotations

from pathlib import Path
from typing import Any

try:
    from manifest import load_manifest, validate_manifest
except ModuleNotFoundError:
    from tools.manifest import load_manifest, validate_manifest


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def profile_manifest(profile: str, root: Path | None = None) -> dict[str, Any]:
    repo = root or repository_root()
    path = repo / "platforms" / f"{profile}.yaml"
    if not path.is_file():
        raise ValueError(f"未知 profile 或清单不存在: {profile}")
    manifest = load_manifest(path)
    validate_manifest(manifest)
    if manifest.get("name") != profile:
        raise ValueError(f"profile 参数与清单 name 不一致: {profile}")
    return manifest


def dependency_dir(root: Path, dependency: str) -> Path:
    names = {"sdk": "bk_avdk_smp", "solution": "bk_solution_ai"}
    return root / names[dependency]
