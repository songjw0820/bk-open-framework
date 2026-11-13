import subprocess
from pathlib import Path

from tools.bk_manifest import profile_manifest
from tools.bk_fetch import inspect_checkout

ROOT = Path(__file__).resolve().parents[2]


def git(repo: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def make_repo(path: Path, ref: str, content: str = "x") -> str:
    path.mkdir()
    git(path, "-c", "init.defaultBranch=main", "init", "-q")
    (path / "README").write_text(content, encoding="utf-8")
    git(path, "add", ".")
    git(path, "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "-qm", "init")
    git(path, "checkout", "-qB", ref)
    return git(path, "rev-parse", "HEAD")


def test_absent_checkout_is_reported(tmp_path):
    manifest = profile_manifest("bk7259_minimal", ROOT)
    result = inspect_checkout(tmp_path / "missing", manifest["sdk"])
    assert not result.ok
    assert "不存在" in result.message or "missing" in result.message


def test_wrong_ref_and_sha_are_reported(tmp_path):
    repo = tmp_path / "sdk"
    actual_sha = make_repo(repo, "main")
    dependency = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": "0" * 40}
    result = inspect_checkout(repo, dependency)
    assert not result.ok
    assert "ref" in result.message or "SHA" in result.message
    assert actual_sha not in dependency["sha"]


def test_correct_ref_with_wrong_sha_reports_sha_mismatch(tmp_path):
    repo = tmp_path / "sdk"
    actual_sha = make_repo(repo, "release/v4.0.1.13")
    dependency = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": "0" * 40}
    result = inspect_checkout(repo, dependency)
    assert not result.ok
    assert "SHA 不匹配" in result.message
    assert actual_sha in result.message


def test_valid_paired_checkouts_are_reported(tmp_path):
    sdk = tmp_path / "bk_avdk_smp"
    solution = tmp_path / "bk_solution_ai"
    sdk_sha = make_repo(sdk, "release/v4.0.1.13")
    solution_sha = make_repo(solution, "release/v4.0.1.13")
    sdk_dep = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": sdk_sha}
    solution_dep = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": solution_sha}
    assert inspect_checkout(sdk, sdk_dep).ok
    assert inspect_checkout(solution, solution_dep).ok


def test_detached_tag_checkout_is_accepted(tmp_path):
    repo = tmp_path / "bk_avdk_smp"
    sha = make_repo(repo, "main")
    git(repo, "-c", "user.name=Test", "-c", "user.email=test@example.com", "tag", "-a", "release/v4.0.1.13", "-m", "release")
    git(repo, "checkout", "-q", "--detach", "release/v4.0.1.13")
    dependency = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": sha}
    result = inspect_checkout(repo, dependency)
    assert result.ok, result.message


def test_detached_head_not_on_locked_tag_is_rejected(tmp_path):
    repo = tmp_path / "bk_avdk_smp"
    sha = make_repo(repo, "main")
    git(repo, "checkout", "-q", "--detach", "HEAD")
    dependency = {"repository": "unused", "ref": "release/v4.0.1.13", "sha": sha}
    result = inspect_checkout(repo, dependency)
    assert not result.ok
    assert "detached HEAD" in result.message
