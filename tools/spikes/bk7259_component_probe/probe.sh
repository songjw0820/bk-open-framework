#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: probe.sh --solution-dir PATH [--sdk-dir PATH] [--build] [--docker]

Without --build, validate AP/CP entry points and print the expected artifact path.
With --build, invoke the Armino project build (host or Docker via dbuild.sh) and require all-app.bin.
USAGE
}

solution_dir=""
sdk_dir=""
run_build=0
use_docker=${BK_USE_DOCKER:-0}
while [ "$#" -gt 0 ]; do
  case "$1" in
    --solution-dir) solution_dir=${2:?missing value}; shift 2 ;;
    --sdk-dir) sdk_dir=${2:?missing value}; shift 2 ;;
    --build) run_build=1; shift ;;
    --docker) use_docker=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$solution_dir" ]; then
  echo "ERROR: --solution-dir is required" >&2
  exit 2
fi
project_dir="$solution_dir/projects/beken_robot"
for required in "$project_dir/Makefile" "$project_dir/CMakeLists.txt" "$project_dir/ap/CMakeLists.txt" "$project_dir/cp/CMakeLists.txt" "$project_dir/cp/config/bk7259/usr_gpio_cfg.h"; do
  if [ ! -f "$required" ]; then
    echo "ERROR: missing required entry point or board file: $required" >&2
    exit 3
  fi
done

artifact="$project_dir/build/bk7259/beken_robot/package/all-app.bin"
printf '%s\n' "AP entry: $project_dir/ap/CMakeLists.txt" "CP entry: $project_dir/cp/CMakeLists.txt" "Project entry: $project_dir/Makefile" "Expected artifact: $artifact"

if [ "$run_build" -eq 0 ]; then
  printf '%s\n' "PASS: entry-point inspection completed"
  exit 0
fi

if [ -z "$sdk_dir" ]; then
  echo "ERROR: --sdk-dir is required with --build" >&2
  exit 4
fi
if [ ! -d "$sdk_dir" ]; then
  echo "ERROR: SDK directory does not exist: $sdk_dir" >&2
  exit 5
fi

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
sdk_dir=$(cd "$sdk_dir" && pwd)
solution_dir=$(cd "$solution_dir" && pwd)
project_dir=$(cd "$project_dir" && pwd)

ap_lib="$project_dir/build/bk7259/beken_robot/bk7259_ap/armino/bk_open_framework/libbk_open_framework.a"
cp_lib="$project_dir/build/bk7259/beken_robot/bk7259/armino/bk_open_framework/libbk_open_framework.a"

if [ "$use_docker" -eq 1 ]; then
  SDK_DIR="$sdk_dir" SOLUTION_DIR="$solution_dir" PROJECT_DIR="projects/beken_robot" \
    "$repo_root/dbuild.sh" make bk7259
else
  if [ -z "${BK_TOOLCHAIN_PATH:-}" ]; then
    for candidate in \
      "/home/antigravity/toolchains/ccache-bin" \
      "/home/antigravity/toolchains/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin" \
      "/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin"; do
      if [ -x "$candidate/arm-none-eabi-gcc" ]; then
        export BK_TOOLCHAIN_PATH="$candidate"
        break
      fi
    done
  fi

  if [ -d "/tmp/venv/bin" ]; then
    export PATH="/tmp/venv/bin:$PATH"
  fi

  ap_comp_dir="$project_dir/ap/components/bk_open_framework"
  cp_comp_dir="$project_dir/cp/components/bk_open_framework"
  cleanup_staged_components() {
    rm -rf "$ap_comp_dir" "$cp_comp_dir"
    rmdir "$project_dir/ap/components" 2>/dev/null || true
    rmdir "$project_dir/cp/components" 2>/dev/null || true
  }
  trap cleanup_staged_components EXIT

  mkdir -p "$ap_comp_dir" "$cp_comp_dir"
  cat > "$ap_comp_dir/CMakeLists.txt" <<EOF
set(BK_OPEN_FRAMEWORK_ROOT "$repo_root")
include("$repo_root/tools/spikes/bk7259_component_probe/bk_open_framework.cmake")
EOF
  cp "$ap_comp_dir/CMakeLists.txt" "$cp_comp_dir/CMakeLists.txt"

  for sub_build in \
    "$project_dir/build/bk7259/beken_robot/bk7259_ap" \
    "$project_dir/build/bk7259/beken_robot/bk7259"; do
    if [ ! -d "$sub_build/armino/bk_open_framework" ] || \
       { [ -f "$sub_build/CMakeCache.txt" ] && grep -q "/bk-open-framework" "$sub_build/CMakeCache.txt"; }; then
      rm -f "$sub_build/CMakeCache.txt"
    fi
  done

  "$repo_root/tools/armino_make.sh" -C "$project_dir" bk7259 SDK_DIR="$sdk_dir"
fi

for built_lib in "$ap_lib" "$cp_lib"; do
  if [ ! -s "$built_lib" ]; then
    echo "ERROR: framework component library missing or empty: $built_lib" >&2
    exit 6
  fi
done
if [ ! -s "$artifact" ]; then
  echo "ERROR: build completed without a non-empty expected artifact: $artifact" >&2
  exit 6
fi
printf '%s\n' "AP framework archive: $ap_lib" "CP framework archive: $cp_lib" "PASS: AP/CP build and all-app.bin artifact verified"
