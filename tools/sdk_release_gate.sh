#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

if [[ -z "${BK_DEPS_ROOT:-}" ]]; then
    echo "ERROR: BK_DEPS_ROOT is not set. This is a manual SDK/AP-CP release gate and cannot run in the ordinary host CI job." >&2
    exit 2
fi

deps_root=$(cd "$BK_DEPS_ROOT" 2>/dev/null && pwd) || {
    echo "ERROR: BK_DEPS_ROOT does not exist: $BK_DEPS_ROOT" >&2
    exit 3
}
sdk_dir="$deps_root/bk_avdk_smp"
solution_dir="$deps_root/bk_solution_ai"

for checkout in "$sdk_dir" "$solution_dir"; do
    if [[ ! -e "$checkout/.git" ]]; then
        echo "ERROR: locked checkout is missing: $checkout" >&2
        echo "       provide matching SDK and solution checkouts under BK_DEPS_ROOT" >&2
        exit 4
    fi
done

"$repo_root/tools/bk" doctor --profile bk7259_robot --root "$deps_root"
"$repo_root/tools/spikes/bk7259_component_probe/probe.sh" \
    --solution-dir "$solution_dir" \
    --sdk-dir "$sdk_dir" \
    --build

project_dir="$solution_dir/projects/beken_robot"
artifact="$project_dir/build/bk7259/beken_robot/package/all-app.bin"
if [[ ! -s "$artifact" ]]; then
    echo "ERROR: AP/CP build did not produce a non-empty all-app.bin: $artifact" >&2
    exit 5
fi

if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$artifact"
else
    shasum -a 256 "$artifact"
fi
echo "PASS: locked SDK doctor, AP/CP probe/build and all-app.bin verification completed"
