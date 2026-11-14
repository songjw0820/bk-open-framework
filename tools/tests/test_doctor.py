from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
BK_CLI = REPO_ROOT / "tools" / "bk"


def _init_git_repo_with_head(directory: Path) -> str:
    subprocess.run(["git", "init", "-q"], cwd=directory, check=True)
    subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=directory, check=True)
    subprocess.run(["git", "config", "user.name", "Test"], cwd=directory, check=True)
    (directory / "README.md").write_text("test\n", encoding="utf-8")
    subprocess.run(["git", "add", "README.md"], cwd=directory, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "init"], cwd=directory, check=True)
    res = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=directory,
        check=True,
        capture_output=True,
        text=True,
    )
    return res.stdout.strip()


class TestBkDoctorAndFetchCli(unittest.TestCase):
    def test_list_profiles(self):
        res = subprocess.run(
            [sys.executable, str(BK_CLI), "list-profiles"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn("bk7259_minimal", res.stdout)
        self.assertIn("bk7259_robot", res.stdout)

    def test_doctor_minimal_manifest_only(self):
        res = subprocess.run(
            [sys.executable, str(BK_CLI), "doctor", "--profile", "bk7259_minimal"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn("profile=minimal", res.stdout)
        self.assertIn("solution_dependency=none", res.stdout)
        self.assertIn("cp_template=sdk:projects/app/cp", res.stdout)

    def test_doctor_minimal_rejects_solution_dir_argument(self):
        with tempfile.TemporaryDirectory() as tmp:
            res = subprocess.run(
                [
                    sys.executable,
                    str(BK_CLI),
                    "doctor",
                    "--profile",
                    "bk7259_minimal",
                    "--solution-dir",
                    tmp,
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertIn("minimal profile forbids --solution-dir", res.stderr)

    def test_doctor_detects_mismatched_sdk_checkout_sha(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk_dir = Path(tmp) / "sdk"
            sdk_dir.mkdir()
            _init_git_repo_with_head(sdk_dir)
            res = subprocess.run(
                [
                    sys.executable,
                    str(BK_CLI),
                    "doctor",
                    "--profile",
                    "bk7259_minimal",
                    "--sdk-dir",
                    str(sdk_dir),
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertIn("SDK checkout HEAD mismatch", res.stderr)

    def test_fetch_dry_run_minimal_does_not_fetch_solution(self):
        res = subprocess.run(
            [
                sys.executable,
                str(BK_CLI),
                "fetch",
                "--profile",
                "bk7259_minimal",
                "--dry-run",
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn("584b947920c414230f9909375f008e7d6a2c6c6b", res.stdout)
        self.assertNotIn("bk_solution_ai", res.stdout)

    def test_fetch_dry_run_robot_fetches_both_sdk_and_solution(self):
        res = subprocess.run(
            [
                sys.executable,
                str(BK_CLI),
                "fetch",
                "--profile",
                "bk7259_robot",
                "--dry-run",
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn("584b947920c414230f9909375f008e7d6a2c6c6b", res.stdout)
        self.assertIn("a1e865e156e8dc31dfb360219c33a6a6d908ea98", res.stdout)


if __name__ == "__main__":
    unittest.main()
