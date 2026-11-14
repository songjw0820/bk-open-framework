# SPDX-License-Identifier: Apache-2.0
"""Platform manifest loader, validator, and checkout verifier."""

from __future__ import annotations

from pathlib import Path
import re
import subprocess
from typing import Any, Dict, List
import yaml


SHA40_RE = re.compile(r"^[0-9a-f]{40}$")


class ManifestValidationError(ValueError):
    """Raised when a platform manifest violates schema or isolation rules."""


def _validate_repo_entry(entry: Any, label: str) -> None:
    if not isinstance(entry, dict):
        raise ManifestValidationError(f"{label} must be a mapping")
    if "sha" in entry:
        raise ManifestValidationError(
            f"{label} uses ambiguous 'sha' field; use 'tag_object_sha' and 'peeled_commit_sha'"
        )
    for field in ("repository", "ref", "tag_object_sha", "peeled_commit_sha"):
        if field not in entry or not isinstance(entry[field], str) or not entry[field]:
            raise ManifestValidationError(f"{label}.{field} is required")
    for sha_field in ("tag_object_sha", "peeled_commit_sha"):
        if not SHA40_RE.match(entry[sha_field]):
            raise ManifestValidationError(
                f"{label}.{sha_field} must be a 40-char lowercase hex SHA1"
            )


def validate_manifest_dict(data: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(data, dict):
        raise ManifestValidationError("Manifest root must be a YAML mapping")

    for req in ("name", "chip", "board", "profile", "sdk", "cp_template", "verification", "components"):
        if req not in data:
            raise ManifestValidationError(f"Missing required top-level key: {req}")

    profile = data["profile"]
    if profile not in ("minimal", "robot"):
        raise ManifestValidationError(f"Unsupported profile '{profile}' (expected 'minimal' or 'robot')")

    _validate_repo_entry(data["sdk"], "sdk")

    cp_template = data["cp_template"]
    if not isinstance(cp_template, dict) or "source" not in cp_template or "path" not in cp_template:
        raise ManifestValidationError("cp_template must contain 'source' and 'path'")

    verification = data["verification"]
    if not isinstance(verification, dict):
        raise ManifestValidationError("verification must be a mapping")
    if verification.get("sdk_peeled_commit_sha") != data["sdk"]["peeled_commit_sha"]:
        raise ManifestValidationError(
            "verification.sdk_peeled_commit_sha does not match sdk.peeled_commit_sha"
        )

    components = data["components"]
    if not isinstance(components, list) or not components:
        raise ManifestValidationError("components must be a non-empty list")

    if "bk_ota" in components:
        raise ManifestValidationError(
            "bk_ota must remain in deferred_target_components until OTA partition prerequisites are verified"
        )

    if profile == "minimal":
        if data.get("solution") is not None:
            raise ManifestValidationError(
                "minimal profile must not depend on solution (expected solution: null)"
            )
        if cp_template["source"] != "sdk":
            raise ManifestValidationError(
                "minimal profile cp_template.source must be 'sdk'"
            )
        if cp_template["path"] != "projects/app/cp":
            raise ManifestValidationError(
                "minimal profile cp_template.path must be 'projects/app/cp'"
            )
        if data.get("product_components"):
            raise ManifestValidationError(
                "minimal profile must not include product_components"
            )
    elif profile == "robot":
        solution = data.get("solution")
        if solution is None:
            raise ManifestValidationError("robot profile requires a valid 'solution' entry")
        _validate_repo_entry(solution, "solution")
        if solution["ref"] != data["sdk"]["ref"]:
            raise ManifestValidationError(
                f"sdk.ref ({data['sdk']['ref']}) and solution.ref ({solution['ref']}) must match"
            )
        if cp_template["source"] != "solution":
            raise ManifestValidationError(
                "robot profile cp_template.source must be 'solution'"
            )
        if verification.get("solution_peeled_commit_sha") != solution["peeled_commit_sha"]:
            raise ManifestValidationError(
                "verification.solution_peeled_commit_sha does not match solution.peeled_commit_sha"
            )

    return data


def load_manifest(path: Path) -> Dict[str, Any]:
    raw = yaml.safe_load(Path(path).read_text(encoding="utf-8"))
    return validate_manifest_dict(raw)


def list_manifests(platforms_dir: Path) -> List[Path]:
    return sorted(platforms_dir.glob("*.yaml"))


def verify_git_checkout_sha(checkout_dir: Path, expected_peeled_sha: str, label: str) -> str:
    checkout_dir = Path(checkout_dir)
    if not checkout_dir.is_dir():
        raise ManifestValidationError(f"{label} checkout directory does not exist: {checkout_dir}")
    res = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=checkout_dir,
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        raise ManifestValidationError(f"{label} directory is not a valid git repository: {checkout_dir}")
    actual_sha = res.stdout.strip()
    if actual_sha != expected_peeled_sha:
        raise ManifestValidationError(
            f"{label} checkout HEAD mismatch: expected {expected_peeled_sha}, got {actual_sha}"
        )
    return actual_sha
