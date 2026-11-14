#!/usr/bin/env bash
set -euo pipefail

# Compile BK Open Framework + BK7259 SDK/Solution using the official Beken Docker container
# (bekencorp/armino-idk), wrapping tools/spikes/bk7259_component_probe/probe.sh.

DOCKER_IMAGE=${DOCKER_IMAGE:-bekencorp/armino-idk}
DOCKER_IMAGE_VERSION=${DOCKER_IMAGE_VERSION:-}
DOCKER_IMAGE_LOWEST_VERSION=${DOCKER_IMAGE_LOWEST_VERSION:-1.5}
DOCKER_OPTION=${DOCKER_OPTION:-}

FRAMEWORK_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BK_DEPS_ROOT=${BK_DEPS_ROOT:-"$FRAMEWORK_DIR/external"}
SDK_DIR=${SDK_DIR:-"$BK_DEPS_ROOT/bk_avdk_smp"}
SOLUTION_DIR=${SOLUTION_DIR:-"$BK_DEPS_ROOT/bk_solution_ai"}
PROFILE=${BK_PROFILE:-robot}
CONFIG_MODE=${BK_CONFIG_MODE:-both}
CLEAN_BUILD=${BK_CLEAN_BUILD:-1}
EXTRA_PROBE_ARGS=()

usage() {
    cat <<'EOF'
Usage: dbuild.sh [options] [-- extra probe.sh args]

Options:
  --profile minimal|robot     Target profile to build (default: robot, or BK_PROFILE)
  --sdk-dir PATH              Path to bk_avdk_smp checkout (default: $BK_DEPS_ROOT/bk_avdk_smp)
  --solution-dir PATH         Path to bk_solution_ai checkout (default: $BK_DEPS_ROOT/bk_solution_ai)
  --config-mode on|off|both   Kconfig probe mode passed to probe.sh (default: both)
  --no-clean-build            Do not pass --clean-build to probe.sh
  -h, --help                  Show this help message

Environment variables:
  BK_DEPS_ROOT                Root directory containing bk_avdk_smp / bk_solution_ai
  DOCKER_IMAGE                Docker image repository (default: bekencorp/armino-idk)
  DOCKER_IMAGE_VERSION        Explicit Docker image tag (auto-detects >= 1.5 if unset)
  DOCKER_OPTION               Extra options passed to 'docker run'
  BK_DOCKER_SKIP_GROUP_CHECK  Set to 1 to skip Linux docker group membership check
EOF
}

parse_args() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --profile)
                PROFILE=${2:?missing value for --profile}
                shift 2
                ;;
            --sdk-dir)
                SDK_DIR=${2:?missing value for --sdk-dir}
                shift 2
                ;;
            --solution-dir)
                SOLUTION_DIR=${2:?missing value for --solution-dir}
                shift 2
                ;;
            --config-mode)
                CONFIG_MODE=${2:?missing value for --config-mode}
                shift 2
                ;;
            --no-clean-build)
                CLEAN_BUILD=0
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            --)
                shift
                EXTRA_PROBE_ARGS+=("$@")
                break
                ;;
            *)
                echo "ERROR: unknown argument: $1" >&2
                usage >&2
                exit 2
                ;;
        esac
    done

    case "$PROFILE" in
        minimal|robot) ;;
        *)
            echo "ERROR: unsupported --profile '$PROFILE' (expected minimal or robot)" >&2
            exit 2
            ;;
    esac

    case "$CONFIG_MODE" in
        on|off|both) ;;
        *)
            echo "ERROR: invalid --config-mode '$CONFIG_MODE' (expected on, off, or both)" >&2
            exit 2
            ;;
    esac
}

nodocker() {
    cat <<'EOF' >&2

You don't have docker installed in your path.
Please install Docker and pull bekencorp/armino-idk (version >= 1.5):
  docker pull bekencorp/armino-idk:latest

EOF
}

check_docker_exist() {
    if ! command -v docker >/dev/null 2>&1; then
        nodocker
        exit 1
    fi
}

check_linux_user_group() {
    if [ "${BK_DOCKER_SKIP_GROUP_CHECK:-0}" = "1" ]; then
        return
    fi
    local os_name
    os_name=$(uname -s)
    if [ "$os_name" != "Linux" ]; then
        return
    fi
    if [ "$(id -u)" -eq 0 ]; then
        return
    fi
    if ! id -nG | grep -qw "docker"; then
        echo "The current user does not belong to the docker group. Please add the group and try again" >&2
        exit 1
    fi
}

check_docker_run() {
    if ! docker info >/dev/null 2>&1; then
        echo "Docker not started, please read the user manual to start Docker." >&2
        exit 1
    fi
    docker --version
}

compare_versions() {
    local version1=$1
    local version2=$2
    local major1 minor1 major2 minor2
    major1=$(echo "$version1" | cut -d. -f1)
    minor1=$(echo "$version1" | cut -d. -f2)
    major2=$(echo "$version2" | cut -d. -f1)
    minor2=$(echo "$version2" | cut -d. -f2)

    if [ "$major1" -gt "$major2" ]; then
        echo 1
    elif [ "$major1" -lt "$major2" ]; then
        echo -1
    elif [ "$minor1" -gt "$minor2" ]; then
        echo 1
    elif [ "$minor1" -lt "$minor2" ]; then
        echo -1
    else
        echo 0
    fi
}

find_max_version() {
    local max_version=$1
    local version result
    for version in "$@"; do
        result=$(compare_versions "$max_version" "$version")
        if [ "$result" -eq -1 ]; then
            max_version=$version
        fi
    done
    echo "$max_version"
}

check_image_exist() {
    if [ -n "$DOCKER_IMAGE_VERSION" ]; then
        return
    fi
    local images_info
    images_info=$(docker images --format "{{.Repository}} {{.Tag}}" | awk -v img="$DOCKER_IMAGE" '$1 == img && $2 ~ /^[0-9]+\.[0-9]+/ {print $2}')
    if [ -z "$images_info" ]; then
        echo "Docker build image ($DOCKER_IMAGE) does not exist, please install image:" >&2
        echo "  docker pull bekencorp/armino-idk:<tag>" >&2
        echo "  or download from https://dl.bekencorp.com/tools/arminosdk/docker_img/armino-idk" >&2
        exit 1
    fi

    local -a images_info_arr
    read -r -a images_info_arr <<< "$(echo "$images_info" | tr '\n' ' ')"
    local max_version result
    max_version=$(find_max_version "${images_info_arr[@]}")
    result=$(compare_versions "$max_version" "$DOCKER_IMAGE_LOWEST_VERSION")
    if [ "$result" -ge 0 ]; then
        DOCKER_IMAGE_VERSION=$max_version
    else
        echo "Docker image version is outdated. The minimum version is $DOCKER_IMAGE_LOWEST_VERSION" >&2
        exit 1
    fi
}

check_checkouts() {
    if [ ! -f "$SDK_DIR/Makefile" ] && [ ! -f "$SDK_DIR/dbuild.sh" ]; then
        echo "ERROR: SDK directory not found: $SDK_DIR (run './tools/bk fetch --profile bk7259_${PROFILE}' first)" >&2
        exit 1
    fi
    SDK_DIR=$(cd "$SDK_DIR" && pwd)

    if [ "$PROFILE" = "robot" ]; then
        if [ ! -d "$SOLUTION_DIR/projects/beken_robot" ]; then
            echo "ERROR: solution directory not found: $SOLUTION_DIR/projects/beken_robot (run './tools/bk fetch --profile bk7259_robot' first)" >&2
            exit 1
        fi
        SOLUTION_DIR=$(cd "$SOLUTION_DIR" && pwd)
    fi
}

set_docker_option() {
    local arch
    arch=$(uname -m)
    if [ "$arch" != "x86_64" ]; then
        DOCKER_OPTION="${DOCKER_OPTION} --platform linux/amd64"
    fi
    if [ -n "${FLASH_CAPACITY:-}" ]; then
        DOCKER_OPTION="${DOCKER_OPTION} -e FLASH_CAPACITY=${FLASH_CAPACITY}"
    fi
    if [ -n "${PSRAM_CAPACITY:-}" ]; then
        DOCKER_OPTION="${DOCKER_OPTION} -e PSRAM_CAPACITY=${PSRAM_CAPACITY}"
    fi
}

docker_run_build() {
    local -a volume_args=(
        -v "$FRAMEWORK_DIR:/work/framework:ro"
        -v "$SDK_DIR:/work/sdk"
    )
    local -a probe_cmd=(
        ./tools/spikes/bk7259_component_probe/probe.sh
        --profile "$PROFILE"
        --sdk-dir /work/sdk
        --config-mode "$CONFIG_MODE"
        --build
    )

    if [ "$PROFILE" = "robot" ]; then
        volume_args+=(-v "$SOLUTION_DIR:/work/solution:ro")
        probe_cmd+=(--solution-dir /work/solution)
    fi

    if [ "$CLEAN_BUILD" -eq 1 ]; then
        probe_cmd+=(--clean-build)
    fi

    if [ "${#EXTRA_PROBE_ARGS[@]}" -gt 0 ]; then
        probe_cmd+=("${EXTRA_PROBE_ARGS[@]}")
    fi

    # shellcheck disable=SC2086
    docker run $DOCKER_OPTION --rm \
        "${volume_args[@]}" \
        -w /work/framework \
        -u "$(id -u)" \
        "${DOCKER_IMAGE}:${DOCKER_IMAGE_VERSION}" \
        "${probe_cmd[@]}"
}

main() {
    parse_args "$@"
    check_docker_exist
    check_linux_user_group
    check_docker_run
    check_image_exist
    check_checkouts
    set_docker_option
    docker_run_build
}

main "$@"
