#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: probe.sh [--profile minimal|robot] [--sdk-dir PATH] [--solution-dir PATH] [--build]

Profiles:
  minimal  Validate/build against pure SDK project (bk_avdk_smp/projects/app). Does NOT use bk_solution_ai.
  robot    Validate/build against robot solution project (bk_solution_ai/projects/beken_robot). Default if --solution-dir is given.

Without --build, validate AP/CP entry points and print the expected artifact path.
With --build, invoke the Armino project build and require all-app.bin.
USAGE
}

profile=""
solution_dir=""
sdk_dir=""
run_build=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --profile) profile=${2:?missing value}; shift 2 ;;
    --solution-dir) solution_dir=${2:?missing value}; shift 2 ;;
    --sdk-dir) sdk_dir=${2:?missing value}; shift 2 ;;
    --build) run_build=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$profile" ]; then
  if [ -n "$solution_dir" ]; then
    profile="robot"
  else
    profile="minimal"
  fi
fi

case "$profile" in
  minimal)
    if [ -z "$sdk_dir" ]; then
      echo "ERROR: --sdk-dir is required for minimal profile" >&2
      exit 2
    fi
    project_name="app"
    project_dir="$sdk_dir/projects/app"
    ;;
  robot)
    if [ -z "$solution_dir" ]; then
      echo "ERROR: --solution-dir is required for robot profile" >&2
      exit 2
    fi
    project_name="beken_robot"
    project_dir="$solution_dir/projects/beken_robot"
    ;;
  *)
    echo "ERROR: unsupported --profile '$profile' (expected minimal or robot)" >&2
    exit 2
    ;;
esac

for required in \
  "$project_dir/Makefile" \
  "$project_dir/CMakeLists.txt" \
  "$project_dir/ap/CMakeLists.txt" \
  "$project_dir/cp/CMakeLists.txt" \
  "$project_dir/cp/config/bk7259/usr_gpio_cfg.h"; do
  if [ ! -f "$required" ]; then
    echo "ERROR: missing required entry point or board file: $required" >&2
    exit 3
  fi
done

artifact="$project_dir/build/bk7259/$project_name/package/all-app.bin"
printf '%s\n' \
  "Profile: $profile" \
  "AP entry: $project_dir/ap/CMakeLists.txt" \
  "CP entry: $project_dir/cp/CMakeLists.txt" \
  "Project entry: $project_dir/Makefile" \
  "Expected artifact: $artifact"

if [ "$run_build" -eq 0 ]; then
  printf '%s\n' "PASS: entry-point inspection completed for profile '$profile'"
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
make -C "$sdk_dir" bk7259 PROJECT="$project_name" PROJECT_DIR="$project_dir"
if [ ! -f "$artifact" ]; then
  echo "ERROR: build completed without expected artifact: $artifact" >&2
  exit 6
fi
printf '%s\n' "PASS: AP/CP build and all-app.bin artifact verified for profile '$profile'"
