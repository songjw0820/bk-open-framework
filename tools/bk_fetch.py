"""Fetch and inspect locked BK SDK dependencies without third-party packages."""
from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    from bk_manifest import dependency_dir
except ModuleNotFoundError:
    from tools.bk_manifest import dependency_dir


@dataclass
class CheckResult:
    ok: bool
    message: str


def _git(path: Path, *args: str) -> tuple[int, str, str]:
    proc = subprocess.run(["git", "-C", str(path), *args], text=True, capture_output=True)
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def _current_ref(path: Path, ref: str, head_sha: str) -> tuple[str | None, str]:
    """Return the ref name HEAD is on, or None with an error message.

    Upstream BK releases (`release/v4.0.1.x`) are annotated tags, so a clone
    lands on a detached HEAD. A detached HEAD is accepted only when the locked
    ref resolves locally (as a tag) to the same commit.
    """
    code, branch, _ = _git(path, "symbolic-ref", "--quiet", "--short", "HEAD")
    if code == 0:
        return branch, ""
    code, tag_sha, _ = _git(path, "rev-parse", "--verify", "--quiet", f"refs/tags/{ref}^{{commit}}")
    if code == 0 and tag_sha.lower() == head_sha.lower():
        return ref, ""
    return None, "detached HEAD 且锁定 ref 未指向当前提交"


def inspect_checkout(path: Path, dependency: dict[str, Any]) -> CheckResult:
    if not path.exists():
        return CheckResult(False, f"checkout 不存在: {path}")
    if not (path / ".git").exists():
        return CheckResult(False, f"不是 Git checkout: {path}")
    code, sha, err = _git(path, "rev-parse", "HEAD")
    if code != 0:
        return CheckResult(False, f"无法读取 checkout SHA {path}: {err}")
    current, err = _current_ref(path, dependency["ref"], sha)
    if current is None:
        return CheckResult(False, f"无法读取 checkout ref {path}: {err}")
    if current != dependency["ref"]:
        return CheckResult(False, f"ref 不匹配: {path} 当前 {current}, 期望 {dependency['ref']}")
    expected = dependency["sha"].lower()
    if sha.lower() != expected:
        return CheckResult(False, f"SHA 不匹配: {path} 当前 {sha}, 期望 {expected}")
    return CheckResult(True, f"OK {path} ref={current} sha={sha}")


def _ensure_clean(path: Path) -> None:
    code, output, err = _git(path, "status", "--porcelain")
    if code != 0:
        raise RuntimeError(f"无法检查现有 checkout: {path}: {err}")
    if output:
        raise RuntimeError(f"拒绝覆盖有未提交改动的 checkout: {path}")


def fetch_dependency(root: Path, kind: str, dependency: dict[str, Any], dry_run: bool = False) -> Path:
    path = dependency_dir(root, kind)
    if dry_run:
        print(f"DRY-RUN {kind}: {dependency['repository']} {dependency['ref']} {dependency['sha']} -> {path}")
        return path
    root.mkdir(parents=True, exist_ok=True)
    if path.exists():
        _ensure_clean(path)
        code, _, err = _git(path, "remote", "get-url", "origin")
        if code != 0:
            raise RuntimeError(f"现有目录没有 origin 远端: {path}: {err}")
        ref = dependency["ref"]
        code, _, _ = _git(path, "fetch", "origin", f"+refs/tags/{ref}:refs/tags/{ref}")
        if code == 0:
            # Release refs are tags: check out the tagged commit detached.
            code, _, err = _git(path, "checkout", "--detach", f"refs/tags/{ref}")
            if code != 0:
                raise RuntimeError(f"切换 {kind} 到 tag {ref} 失败: {err}")
        else:
            code, _, err = _git(path, "fetch", "origin", ref)
            if code != 0:
                raise RuntimeError(f"获取 {kind} ref 失败: {err}")
            remote_ref = f"origin/{ref}"
            # The checkout is clean at this point, so explicitly move the local
            # branch to the freshly fetched remote tip. This updates stale local
            # commits while never overwriting uncommitted work.
            code, _, err = _git(path, "checkout", "-B", ref, remote_ref)
            if code != 0:
                raise RuntimeError(f"切换 {kind} ref 到远程最新提交失败: {err}")
    else:
        # `--branch` accepts both branches and tags; a shallow clone is enough
        # because only the locked commit is built.
        proc = subprocess.run(
            ["git", "clone", "--depth", "1", "--branch", dependency["ref"], dependency["repository"], str(path)],
            text=True,
            capture_output=True,
        )
        if proc.returncode != 0:
            raise RuntimeError(f"克隆 {kind} 失败: {proc.stderr.strip()}")
    result = inspect_checkout(path, dependency)
    if not result.ok:
        raise RuntimeError(result.message)
    print(result.message)
    return path
