# SPDX-License-Identifier: Apache-2.0
import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _make_executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


class TestDbuildWrapper(unittest.TestCase):
    def test_robot_profile_mounts_volumes_and_invokes_probe(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            bin_dir = tmp_path / "bin"
            bin_dir.mkdir()
            sdk_dir = tmp_path / "bk_avdk_smp"
            sdk_dir.mkdir()
            (sdk_dir / "Makefile").write_text("all:\n", encoding="utf-8")
            solution_dir = tmp_path / "bk_solution_ai"
            (solution_dir / "projects" / "beken_robot").mkdir(parents=True)

            log_file = tmp_path / "docker_run.log"
            _make_executable(
                bin_dir / "docker",
                f"""#!/usr/bin/env bash
set -euo pipefail
case "${{1:-}}" in
  info) exit 0 ;;
  --version) echo "Docker version 27.0.0"; exit 0 ;;
  images)
    printf "bekencorp/armino-idk 1.4\\nbekencorp/armino-idk 1.6\\n"
    exit 0
    ;;
  run)
    printf '%s\\n' "$@" > "{log_file}"
    exit 0
    ;;
esac
exit 1
""",
            )

            env = os.environ.copy()
            env["PATH"] = f"{bin_dir}:{env.get('PATH', '')}"
            env["BK_DOCKER_SKIP_GROUP_CHECK"] = "1"
            env["SDK_DIR"] = str(sdk_dir)
            env["SOLUTION_DIR"] = str(solution_dir)

            result = subprocess.run(
                [str(ROOT / "dbuild.sh"), "--profile", "robot"],
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            args = log_file.read_text(encoding="utf-8").splitlines()
            self.assertIn(f"{ROOT}:/work/framework:ro", args)
            self.assertIn(f"{sdk_dir}:/work/sdk", args)
            self.assertIn(f"{solution_dir}:/work/solution:ro", args)
            self.assertIn("/work/framework", args)
            self.assertIn("bekencorp/armino-idk:1.6", args)
            self.assertIn("./tools/spikes/bk7259_component_probe/probe.sh", args)
            self.assertIn("--profile", args)
            self.assertIn("robot", args)
            self.assertIn("--solution-dir", args)
            self.assertIn("/work/solution", args)
            self.assertIn("--build", args)
            self.assertIn("--clean-build", args)

    def test_minimal_profile_does_not_require_or_mount_solution(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            bin_dir = tmp_path / "bin"
            bin_dir.mkdir()
            sdk_dir = tmp_path / "bk_avdk_smp"
            sdk_dir.mkdir()
            (sdk_dir / "Makefile").write_text("all:\n", encoding="utf-8")

            log_file = tmp_path / "docker_run.log"
            _make_executable(
                bin_dir / "docker",
                f"""#!/usr/bin/env bash
set -euo pipefail
case "${{1:-}}" in
  info) exit 0 ;;
  --version) echo "Docker version 27.0.0"; exit 0 ;;
  images)
    printf "bekencorp/armino-idk 1.5\\n"
    exit 0
    ;;
  run)
    printf '%s\\n' "$@" > "{log_file}"
    exit 0
    ;;
esac
exit 1
""",
            )

            env = os.environ.copy()
            env["PATH"] = f"{bin_dir}:{env.get('PATH', '')}"
            env["BK_DOCKER_SKIP_GROUP_CHECK"] = "1"
            env["SDK_DIR"] = str(sdk_dir)
            env["SOLUTION_DIR"] = str(tmp_path / "nonexistent_solution")

            result = subprocess.run(
                [str(ROOT / "dbuild.sh"), "--profile", "minimal", "--no-clean-build"],
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            args = log_file.read_text(encoding="utf-8").splitlines()
            self.assertIn(f"{sdk_dir}:/work/sdk", args)
            self.assertNotIn("--solution-dir", args)
            self.assertNotIn("--clean-build", args)
            self.assertIn("minimal", args)

    def test_rejects_outdated_docker_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            bin_dir = tmp_path / "bin"
            bin_dir.mkdir()
            sdk_dir = tmp_path / "bk_avdk_smp"
            sdk_dir.mkdir()
            (sdk_dir / "Makefile").write_text("all:\n", encoding="utf-8")

            _make_executable(
                bin_dir / "docker",
                """#!/usr/bin/env bash
case "${1:-}" in
  info) exit 0 ;;
  --version) echo "Docker version 27.0.0"; exit 0 ;;
  images) printf "bekencorp/armino-idk 1.4\\n"; exit 0 ;;
esac
exit 1
""",
            )

            env = os.environ.copy()
            env["PATH"] = f"{bin_dir}:{env.get('PATH', '')}"
            env["BK_DOCKER_SKIP_GROUP_CHECK"] = "1"
            env["SDK_DIR"] = str(sdk_dir)

            result = subprocess.run(
                [str(ROOT / "dbuild.sh"), "--profile", "minimal"],
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("outdated", result.stderr)


if __name__ == "__main__":
    unittest.main()
