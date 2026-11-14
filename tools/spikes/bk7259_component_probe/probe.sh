#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_probe_comp="$script_dir/fixture/bk_probe_comp"
probe_symbol="bk_open_framework_probe_marker"

usage() {
  cat <<'USAGE'
Usage: probe.sh [--profile minimal|robot] --sdk-dir PATH [--solution-dir PATH]
                [--probe-component PATH] [--config-mode on|off|both] [--build]

Profiles:
  minimal  Validate/build against pure SDK project (bk_avdk_smp/projects/app). Does NOT use bk_solution_ai.
  robot    Validate/build against robot solution project (bk_solution_ai/projects/beken_robot). Default if --solution-dir is given.

Options:
  --probe-component PATH     External component fixture to inject into AP and CP (default: fixture/bk_probe_comp).
  --config-mode on|off|both  Verify Kconfig enabled ('on'), disabled ('off'), or both sequentially (default: both).
  --build                    Execute AP/CP builds, verify all-app.bin, and run nm symbol checks on AP (bk7259_ap) and CP (bk7259).
USAGE
}

profile=""
solution_dir=""
sdk_dir=""
probe_comp_dir="$default_probe_comp"
config_mode="both"
run_build=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --profile) profile=${2:?missing value}; shift 2 ;;
    --solution-dir) solution_dir=${2:?missing value}; shift 2 ;;
    --sdk-dir) sdk_dir=${2:?missing value}; shift 2 ;;
    --probe-component) probe_comp_dir=${2:?missing value}; shift 2 ;;
    --config-mode) config_mode=${2:?missing value}; shift 2 ;;
    --build) run_build=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

case "$config_mode" in
  on|off|both) ;;
  *) echo "ERROR: invalid --config-mode '$config_mode' (expected on, off, or both)" >&2; exit 2 ;;
esac

if [ -z "$profile" ]; then
  if [ -n "$solution_dir" ]; then
    profile="robot"
  else
    profile="minimal"
  fi
fi

for comp_req in \
  "$probe_comp_dir/Kconfig" \
  "$probe_comp_dir/CMakeLists.txt" \
  "$probe_comp_dir/include/bk_probe_comp.h" \
  "$probe_comp_dir/src/bk_probe_comp.c"; do
  if [ ! -f "$comp_req" ]; then
    echo "ERROR: missing probe component fixture file: $comp_req" >&2
    exit 3
  fi
done

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

ap_defconfig="$project_dir/ap/config/bk7259_ap/defconfig"
cp_defconfig="$project_dir/cp/config/bk7259/defconfig"

for required in \
  "$project_dir/Makefile" \
  "$project_dir/CMakeLists.txt" \
  "$project_dir/ap/CMakeLists.txt" \
  "$project_dir/cp/CMakeLists.txt" \
  "$ap_defconfig" \
  "$cp_defconfig" \
  "$project_dir/cp/config/bk7259/usr_gpio_cfg.h"; do
  if [ ! -f "$required" ]; then
    echo "ERROR: missing required entry point or board file: $required" >&2
    exit 3
  fi
done

sdk_build_root="${sdk_dir:-<SDK_DIR>}"
build_dir="$sdk_build_root/build/bk7259/$project_name"
artifact="$build_dir/package/all-app.bin"

printf '%s\n' \
  "Profile: $profile" \
  "Probe component: $probe_comp_dir (symbol: $probe_symbol)" \
  "Config mode: $config_mode" \
  "AP entry: $project_dir/ap/CMakeLists.txt" \
  "CP entry: $project_dir/cp/CMakeLists.txt" \
  "AP injection path: $project_dir/ap/components/bk_probe_comp" \
  "CP injection path: $project_dir/cp/components/bk_probe_comp" \
  "Expected AP build dir: $build_dir/bk7259_ap" \
  "Expected CP build dir: $build_dir/bk7259" \
  "Expected artifact: $artifact"

if [ "$run_build" -eq 0 ]; then
  printf '%s\n' "PASS: entry-point and probe fixture inspection completed for profile '$profile'"
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

nm_tool=""
for candidate in arm-none-eabi-nm llvm-nm nm; do
  if command -v "$candidate" >/dev/null 2>&1; then
    nm_tool="$candidate"
    break
  fi
done
if [ -z "$nm_tool" ]; then
  echo "ERROR: no nm tool (arm-none-eabi-nm / llvm-nm / nm) found in PATH" >&2
  exit 5
fi

backup_dir="$(mktemp -d)"
cp "$ap_defconfig" "$backup_dir/ap_defconfig.bak"
cp "$cp_defconfig" "$backup_dir/cp_defconfig.bak"
cp "$project_dir/ap/CMakeLists.txt" "$backup_dir/ap_CMakeLists.bak"
cp "$project_dir/cp/CMakeLists.txt" "$backup_dir/cp_CMakeLists.bak"

cleanup() {
  cp "$backup_dir/ap_defconfig.bak" "$ap_defconfig" 2>/dev/null || true
  cp "$backup_dir/cp_defconfig.bak" "$cp_defconfig" 2>/dev/null || true
  cp "$backup_dir/ap_CMakeLists.bak" "$project_dir/ap/CMakeLists.txt" 2>/dev/null || true
  cp "$backup_dir/cp_CMakeLists.bak" "$project_dir/cp/CMakeLists.txt" 2>/dev/null || true
  rm -rf "$project_dir/ap/components/bk_probe_comp" "$project_dir/cp/components/bk_probe_comp" "$backup_dir"
}
trap cleanup EXIT

# Per bk_avdk_smp/tools/build_tools/cmake/project.cmake:266-277:
# EXTRA_COMPONENTS_DIRS is only scanned when sub_sys == "ap", whereas
# ${PROJECT_DIR}/${sub_sys}/components is scanned for both "ap" and "cp".
mkdir -p "$project_dir/ap/components" "$project_dir/cp/components"
rm -rf "$project_dir/ap/components/bk_probe_comp" "$project_dir/cp/components/bk_probe_comp"
cp -R "$probe_comp_dir" "$project_dir/ap/components/bk_probe_comp"
cp -R "$probe_comp_dir" "$project_dir/cp/components/bk_probe_comp"

for side_cmake in "$project_dir/ap/CMakeLists.txt" "$project_dir/cp/CMakeLists.txt"; do
  sed -i 's/armino_component_register(/list(APPEND priv_req bk_probe_comp)\narmino_component_register(/' "$side_cmake"
done

check_symbol_in_side() {
  local side_build_dir="$1"
  local expect_present="$2"
  local side_name="$3"

  if [ ! -d "$side_build_dir" ]; then
    echo "ERROR: missing $side_name build directory: $side_build_dir" >&2
    exit 6
  fi

  local found_in_elf=0
  local found_in_any=0
  while IFS= read -r -d '' elf_file; do
    if "$nm_tool" "$elf_file" 2>/dev/null | grep -q "$probe_symbol"; then
      found_in_elf=1
      found_in_any=1
      break
    fi
  done < <(find "$side_build_dir" -name "*.elf" -print0)

  if [ "$found_in_any" -eq 0 ]; then
    while IFS= read -r -d '' obj_file; do
      if "$nm_tool" "$obj_file" 2>/dev/null | grep -q "$probe_symbol"; then
        found_in_any=1
        break
      fi
    done < <(find "$side_build_dir" \( -name "libbk_probe_comp.a" -o -name "*bk_probe_comp*.o" -o -name "*bk_probe_comp*.obj" \) -print0)
  fi

  if [ "$expect_present" -eq 1 ] && [ "$found_in_elf" -eq 0 ]; then
    echo "ERROR: symbol '$probe_symbol' missing in $side_name ELF under $side_build_dir when CONFIG_BK_OPEN_FRAMEWORK_PROBE=y" >&2
    exit 7
  fi
  if [ "$expect_present" -eq 0 ] && [ "$found_in_any" -eq 1 ]; then
    echo "ERROR: symbol '$probe_symbol' still present in $side_name build ($side_build_dir) when CONFIG_BK_OPEN_FRAMEWORK_PROBE=n" >&2
    exit 8
  fi
}

run_single_mode() {
  local mode="$1"
  cp "$backup_dir/ap_defconfig.bak" "$ap_defconfig"
  cp "$backup_dir/cp_defconfig.bak" "$cp_defconfig"

  if [ "$mode" = "on" ]; then
    printf '\nCONFIG_BK_OPEN_FRAMEWORK_PROBE=y\n' >> "$ap_defconfig"
    printf '\nCONFIG_BK_OPEN_FRAMEWORK_PROBE=y\n' >> "$cp_defconfig"
    expect_sym=1
  else
    printf '\n# CONFIG_BK_OPEN_FRAMEWORK_PROBE is not set\n' >> "$ap_defconfig"
    printf '\n# CONFIG_BK_OPEN_FRAMEWORK_PROBE is not set\n' >> "$cp_defconfig"
    expect_sym=0
  fi

  rm -rf "$build_dir"
  make -C "$sdk_dir" bk7259 PROJECT="$project_name" PROJECT_DIR="$project_dir"

  if [ ! -f "$artifact" ]; then
    echo "ERROR: [$mode] build completed without expected artifact: $artifact" >&2
    exit 6
  fi

  # Per tools/build_tools/build.sh, AP target is bk7259_ap and CP target is bk7259
  check_symbol_in_side "$build_dir/bk7259_ap" "$expect_sym" "AP (bk7259_ap)"
  check_symbol_in_side "$build_dir/bk7259" "$expect_sym" "CP (bk7259)"
  printf '%s\n' "PASS: [$mode] AP/CP component probe, symbol check ($nm_tool), and $artifact verified"
}

if [ "$config_mode" = "both" ]; then
  run_single_mode "on"
  run_single_mode "off"
else
  run_single_mode "$config_mode"
fi

printf '%s\n' "PASS: AP/CP build, Kconfig on/off gating, and all-app.bin artifact verified for profile '$profile'"
