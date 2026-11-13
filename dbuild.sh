#!/usr/bin/env bash
set -euo pipefail

# Compile BK Open Framework + BK7259 SDK/Solution using the official Beken Docker container
# (bekencorp/armino-idk), modeled on bk_avdk_smp/dbuild.sh and beken_robot/dbuild.sh.

DOCKER_IMAGE=${DOCKER_IMAGE:-bekencorp/armino-idk}
DOCKER_IMAGE_VERSION=${DOCKER_IMAGE_VERSION:-}
DOCKER_IMAGE_LOWEST_VERSION=${DOCKER_IMAGE_LOWEST_VERSION:-1.5}
DOCKER_OPTION=${DOCKER_OPTION:-}

FRAMEWORK_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BK_DEPS_ROOT=${BK_DEPS_ROOT:-"$FRAMEWORK_DIR/.bk-deps"}
SDK_DIR=${SDK_DIR:-"$BK_DEPS_ROOT/bk_avdk_smp"}
SOLUTION_DIR=${SOLUTION_DIR:-"$BK_DEPS_ROOT/bk_solution_ai"}
PROJECT_DIR=${PROJECT_DIR:-"projects/beken_robot"}

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
    if [ ! -f "$SDK_DIR/dbuild.sh" ] && [ ! -f "$SDK_DIR/Makefile" ]; then
        echo "ERROR: SDK directory not found: $SDK_DIR (run './tools/bk fetch --profile bk7259_robot' first)" >&2
        exit 1
    fi
    if [ ! -d "$SOLUTION_DIR/$PROJECT_DIR" ]; then
        echo "ERROR: project directory not found: $SOLUTION_DIR/$PROJECT_DIR (run './tools/bk fetch --profile bk7259_robot' first)" >&2
        exit 1
    fi
    SDK_DIR=$(cd "$SDK_DIR" && pwd)
    SOLUTION_DIR=$(cd "$SOLUTION_DIR" && pwd)
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

stage_framework_components() {
    local host_project_dir="$SOLUTION_DIR/$PROJECT_DIR"
    ap_comp_dir="$host_project_dir/ap/components/bk_open_framework"
    cp_comp_dir="$host_project_dir/cp/components/bk_open_framework"

    mkdir -p "$ap_comp_dir" "$cp_comp_dir"
    cat > "$ap_comp_dir/CMakeLists.txt" <<'EOF'
set(BK_OPEN_FRAMEWORK_ROOT "/bk-open-framework")
include("/bk-open-framework/tools/spikes/bk7259_component_probe/bk_open_framework.cmake")
EOF
    cp "$ap_comp_dir/CMakeLists.txt" "$cp_comp_dir/CMakeLists.txt"

    for sub_build in \
        "$host_project_dir/build/bk7259/beken_robot/bk7259_ap" \
        "$host_project_dir/build/bk7259/beken_robot/bk7259"; do
        if [ ! -d "$sub_build/armino/bk_open_framework" ] || \
           { [ -f "$sub_build/CMakeCache.txt" ] && ! grep -q "/armino" "$sub_build/CMakeCache.txt"; }; then
            rm -f "$sub_build/CMakeCache.txt"
        fi
    done
}

cleanup_staged_components() {
    if [ -n "${ap_comp_dir:-}" ] && [ -n "${cp_comp_dir:-}" ]; then
        rm -rf "$ap_comp_dir" "$cp_comp_dir"
        rmdir "$(dirname "$ap_comp_dir")" 2>/dev/null || true
        rmdir "$(dirname "$cp_comp_dir")" 2>/dev/null || true
    fi
}

docker_run_build() {
    if [ "$#" -eq 0 ]; then
        set -- make bk7259
    fi
    # shellcheck disable=SC2086
    docker run $DOCKER_OPTION --rm \
        -v "$SDK_DIR:/armino" \
        -v "$SOLUTION_DIR:/solution" \
        -v "$FRAMEWORK_DIR:/bk-open-framework" \
        -w "/solution/$PROJECT_DIR" \
        -u "$(id -u)" \
        "${DOCKER_IMAGE}:${DOCKER_IMAGE_VERSION}" \
        "$@" SDK_DIR=/armino
}

main() {
    check_docker_exist
    check_linux_user_group
    check_docker_run
    check_image_exist
    check_checkouts
    set_docker_option
    trap cleanup_staged_components EXIT
    stage_framework_components
    docker_run_build "$@"
}

main "$@"
