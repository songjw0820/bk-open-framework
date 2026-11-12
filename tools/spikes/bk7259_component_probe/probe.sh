#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: probe.sh --solution-dir PATH [--sdk-dir PATH] [--build]

Without --build, validate AP/CP entry points and print the expected artifact path.
With --build, invoke the Armino project build and require all-app.bin.
USAGE
}

solution_dir=""
sdk_dir=""
run_build=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --solution-dir) solution_dir=${2:?missing value}; shift 2 ;;
    --sdk-dir) sdk_dir=${2:?missing value}; shift 2 ;;
    --build) run_build=1; shift ;;
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
make -C "$sdk_dir" bk7259 PROJECT=beken_robot PROJECT_DIR="$project_dir"
if [ ! -f "$artifact" ]; then
  echo "ERROR: build completed without expected artifact: $artifact" >&2
  exit 6
fi
printf '%s\n' "PASS: AP/CP build and all-app.bin artifact verified"
