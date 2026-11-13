import os
import stat
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _make_executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


def test_dbuild_stages_components_mounts_volumes_and_cleans_up(tmp_path):
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    sdk_dir = tmp_path / "bk_avdk_smp"
    sdk_dir.mkdir()
    (sdk_dir / "dbuild.sh").write_text("#!/bin/sh\n", encoding="utf-8")
    solution_dir = tmp_path / "bk_solution_ai"
    project_dir = solution_dir / "projects" / "beken_robot"
    (project_dir / "ap").mkdir(parents=True)
    (project_dir / "cp").mkdir(parents=True)

    log_file = tmp_path / "docker_run.log"
    staged_ap_file = tmp_path / "staged_ap.cmake"
    staged_cp_file = tmp_path / "staged_cp.cmake"

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
    cp "{project_dir}/ap/components/bk_open_framework/CMakeLists.txt" "{staged_ap_file}"
    cp "{project_dir}/cp/components/bk_open_framework/CMakeLists.txt" "{staged_cp_file}"
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
        [str(ROOT / "dbuild.sh"), "make", "bk7259"],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    args = log_file.read_text(encoding="utf-8").splitlines()
    assert f"{sdk_dir}:/armino" in args
    assert f"{solution_dir}:/solution" in args
    assert f"{ROOT}:/bk-open-framework" in args
    assert "/solution/projects/beken_robot" in args
    assert "bekencorp/armino-idk:1.6" in args
    assert "SDK_DIR=/armino" in args

    assert '/bk-open-framework/tools/spikes/bk7259_component_probe/bk_open_framework.cmake' in staged_ap_file.read_text(encoding="utf-8")
    assert '/bk-open-framework/tools/spikes/bk7259_component_probe/bk_open_framework.cmake' in staged_cp_file.read_text(encoding="utf-8")
    assert not (project_dir / "ap" / "components").exists()
    assert not (project_dir / "cp" / "components").exists()


def test_dbuild_rejects_outdated_docker_image(tmp_path):
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    sdk_dir = tmp_path / "bk_avdk_smp"
    sdk_dir.mkdir()
    (sdk_dir / "dbuild.sh").write_text("#!/bin/sh\n", encoding="utf-8")
    solution_dir = tmp_path / "bk_solution_ai"
    (solution_dir / "projects" / "beken_robot").mkdir(parents=True)

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
    env["SOLUTION_DIR"] = str(solution_dir)

    result = subprocess.run(
        [str(ROOT / "dbuild.sh"), "make", "bk7259"],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode != 0
    assert "outdated" in result.stderr
