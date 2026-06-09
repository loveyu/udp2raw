#!/usr/bin/env bash
# Build libudp2raw_plugin.so for all Android ABIs using the NDK.
#
# Usage:
#   ./build_android.sh [NDK_PATH] [API_LEVEL] [OUTPUT_DIR]
#
# Defaults:
#   NDK_PATH    = $ANDROID_NDK_HOME or $ANDROID_NDK
#   API_LEVEL   = 33
#   OUTPUT_DIR  = ./output/jniLibs
#
# The script produces an 'udp2raw-android-jniLibs.zip' that the Android
# app's gradle task can download and unpack.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

NDK_PATH="${1:-${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}}"
API_LEVEL="${2:-33}"
OUTPUT_DIR="${3:-${SCRIPT_DIR}/output/jniLibs}"

# Auto-detect NDK from ~/Android/Sdk/ndk/ if not set
if [[ -z "$NDK_PATH" ]]; then
    HOME_SDK="$HOME/Android/Sdk"
    if [[ -d "$HOME_SDK/ndk" ]]; then
        NDK_PATH="$(ls -d "$HOME_SDK/ndk/"* 2>/dev/null | sort -V | tail -1)"
    fi
fi

if [[ -z "$NDK_PATH" ]]; then
    echo "ERROR: Android NDK not found."
    echo "  Set ANDROID_NDK_HOME, pass the NDK path as first argument,"
    echo "  or install NDK under ~/Android/Sdk/ndk/."
    exit 1
fi

CMAKE="${NDK_PATH}/cmake"
# Prefer NDK-bundled cmake if available
if command -v cmake &>/dev/null; then
    CMAKE="cmake"
fi

ABIS=("arm64-v8a" "armeabi-v7a" "x86_64" "x86")

for ABI in "${ABIS[@]}"; do
    echo "=== Building $ABI ==="
    BUILD_DIR="${SCRIPT_DIR}/build_${ABI}"
    LIB_OUT="${OUTPUT_DIR}/${ABI}"

    mkdir -p "$BUILD_DIR" "$LIB_OUT"

    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="${ABI}" \
        -DANDROID_PLATFORM="android-${API_LEVEL}" \
        -DANDROID_STL="c++_static" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="${LIB_OUT}"

    cmake --build "${BUILD_DIR}" --target udp2raw_plugin -j"$(nproc)"

    # Rename to libudp2raw_plugin.so
    mv -f "${LIB_OUT}/libudp2raw_plugin.so" "${LIB_OUT}/libudp2raw_plugin.so" 2>/dev/null || true
    echo "  -> ${LIB_OUT}/libudp2raw_plugin.so"
done

# Create the distributable zip
ZIP_NAME="udp2raw-android-jniLibs.zip"
ZIP_PATH="${SCRIPT_DIR}/${ZIP_NAME}"
echo ""
echo "=== Creating ${ZIP_NAME} ==="
cd "${OUTPUT_DIR}/.."
zip -r "${ZIP_PATH}" "jniLibs/"
echo "  -> ${ZIP_PATH}"
echo ""
echo "Done. Upload ${ZIP_PATH} as a GitHub release asset."
