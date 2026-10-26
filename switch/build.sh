#!/usr/bin/env bash
# Builds switch/build/rs64recomp.nro.
# First generate the code from your ROM and build the patches (see README.md).
set -euo pipefail
cd "$(dirname "$0")/.."

# The Switch changes to pinned submodules live in switch/patches, so the submodules stay on
# their upstream commits. Applying twice is a no-op.
apply_patch() {  # <submodule dir> <patch file>
    if git -C "$1" apply --reverse --check "$PWD/$2" 2>/dev/null; then
        return
    fi
    git -C "$1" apply "$PWD/$2"
}
apply_patch lib/N64ModernRuntime switch/patches/n64modernruntime.patch
apply_patch lib/N64ModernRuntime/N64Recomp switch/patches/n64recomp.patch
apply_patch lib/GLideN64 switch/patches/gliden64.patch

[ -f patches/patches.bin ] || { echo "patches/patches.bin missing: build the patches first (see README.md)" >&2; exit 1; }

# patches_bin.c/.h are what upstream's file_to_c tool produces from patches.bin.
python3 - <<'PY'
from pathlib import Path
data = Path("patches/patches.bin").read_bytes()
body = ", ".join(str(b - 256 if b > 127 else b) for b in data)
Path("RecompiledPatches/patches_bin.c").write_text(
    f"extern const char rs64_patches_bin[{len(data)}];\n"
    f"const char rs64_patches_bin[{len(data)}] = {{{body}}};\n")
Path("RecompiledPatches/patches_bin.h").write_text(
    f"#pragma once\nextern \"C\" const char rs64_patches_bin[{len(data)}];\n")
PY

# CVNX_DEV=1 builds the remote test harness into switch/build-dev, a separate tree so a release build
# (switch/build) can never pick it up.
BUILD_DIR=build
EXTRA_ARGS=""
if [ "${CVNX_DEV:-0}" = 1 ]; then
    BUILD_DIR=build-dev
fi

# Uses the devkitPro install in $DEVKITPRO (default /opt/devkitpro) with switch-sdl2, switch-mesa,
# switch-libdrm_nouveau and switch-glad.
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
"$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake" -S switch -B "switch/$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release $EXTRA_ARGS
cmake --build "switch/$BUILD_DIR" -j"$(nproc)"
ls -la "switch/$BUILD_DIR/rs64recomp.nro"
