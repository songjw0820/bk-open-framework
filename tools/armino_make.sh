#!/usr/bin/env bash
# make wrapper for Armino SMP builds with a non-default toolchain location.
#
# Armino's soc_config.mk hardcodes COMPILER_TOOLCHAIN_PATH to
# /opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin, and
# build_smp_parallel.sh clears MAKEFLAGS before spawning the AP/CP sub-makes,
# so a plain command-line override is lost. The script does honor $MAKE, so
# pass this wrapper as MAKE=... and every (sub-)make gets the override.
set -euo pipefail

if [[ -n "${BK_TOOLCHAIN_PATH:-}" ]]; then
    exec make "$@" "COMPILER_TOOLCHAIN_PATH=${BK_TOOLCHAIN_PATH}" "MAKE=$0"
fi
exec make "$@"
