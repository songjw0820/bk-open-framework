# SPDX-License-Identifier: Apache-2.0
"""Fetch or print deterministic checkout commands for locked SDK/solution repositories."""

from __future__ import annotations

from pathlib import Path
import subprocess
from typing import Any, Dict, List

from tools.bk_manifest import verify_git_checkout_sha


def build_fetch_plan(manifest: Dict[str, Any], dest_root: Path) -> List[Dict[str, str]]:
    dest_root = Path(dest_root)
    sdk = manifest["sdk"]
    plan = [
        {
            "role": "sdk",
            "repository": sdk["repository"],
            "ref": sdk["ref"],
            "tag_object_sha": sdk["tag_object_sha"],
            "peeled_commit_sha": sdk["peeled_commit_sha"],
            "dest": str(dest_root / "bk_avdk_smp"),
        }
    ]
    if manifest["profile"] == "robot" and manifest.get("solution") is not None:
        sol = manifest["solution"]
        plan.append(
            {
                "role": "solution",
                "repository": sol["repository"],
                "ref": sol["ref"],
                "tag_object_sha": sol["tag_object_sha"],
                "peeled_commit_sha": sol["peeled_commit_sha"],
                "dest": str(dest_root / "bk_solution_ai"),
            }
        )
    return plan


def execute_fetch_plan(plan: List[Dict[str, str]], dry_run: bool = False) -> List[str]:
    messages: List[str] = []
    for item in plan:
        dest = Path(item["dest"])
        repo = item["repository"]
        ref = item["ref"]
        peeled = item["peeled_commit_sha"]
        summary = (
            f"[{item['role']}] repo={repo} ref={ref} "
            f"peeled_commit_sha={peeled} dest={dest}"
        )
        messages.append(summary)
        if dry_run:
            continue

        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(
                ["git", "clone", "--no-checkout", repo, str(dest)],
                check=True,
            )
        subprocess.run(
            ["git", "fetch", "--tags", "origin", ref],
            cwd=dest,
            check=True,
        )
        subprocess.run(
            ["git", "checkout", "--detach", peeled],
            cwd=dest,
            check=True,
        )
        verify_git_checkout_sha(dest, peeled, item["role"].upper())
    return messages
