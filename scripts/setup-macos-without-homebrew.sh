#!/usr/bin/env bash
set -euo pipefail

# One-time local setup for building Photos by Larry on a Mac too old for
# Homebrew's current Qt6/OpenCV bottles - written for macOS 12 Monterey
# specifically, but works for any macOS version Homebrew's own supported
# range has moved past (see the README's macOS section for why
# `brew install qt6 opencv` stops working as your OS ages, even though
# nothing about your machine changed).
#
# Unlike Homebrew, neither step here depends on what macOS version you're
# currently running: Qt's official binaries are fetched pinned to a
# version whose documented minimum macOS is 12, and OpenCV is built from
# source with that same floor set explicitly via
# CMAKE_OSX_DEPLOYMENT_TARGET - so this works regardless of how far
# Homebrew's own minimum has since moved past your OS.
#
# Run this once. It prints the `cmake -B build ...` command to use
# afterward - that (and `cmake --build build`) is what you'll actually run
# day to day; this script doesn't need to be re-run on every rebuild.

QT_VERSION="${QT_VERSION:-6.8.0}"                       # LTS; documented minimum macOS is 12
MACOS_DEPLOYMENT_TARGET="${MACOS_DEPLOYMENT_TARGET:-12.0}"
OPENCV_VERSION="${OPENCV_VERSION:-4.10.0}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${REPO_ROOT}/.deps"
OPENCV_INSTALL_DIR="${DEPS_DIR}/opencv-install"
QT_INSTALL_DIR="${HOME}/Qt" # aqtinstall's own default layout convention

echo "== Installing aqtinstall (Qt's own official binaries, not Homebrew's) =="
python3 -m pip install --user --upgrade aqtinstall

echo "== Installing Qt ${QT_VERSION} (macOS ${MACOS_DEPLOYMENT_TARGET}+) =="
if [ -d "${QT_INSTALL_DIR}/${QT_VERSION}" ]; then
    echo "Qt ${QT_VERSION} already present at ${QT_INSTALL_DIR}/${QT_VERSION}, skipping."
else
    # "clang_64" is the standing arch identifier for Qt's macOS desktop kit.
    # If this errors on an unrecognized arch, run:
    #   python3 -m aqt list-qt mac desktop --long-modules "${QT_VERSION}"
    # to see what aqt actually expects for this version, and override with
    # QT_ARCH=<value> ./scripts/setup-macos-without-homebrew.sh
    python3 -m aqt install-qt mac desktop "${QT_VERSION}" "${QT_ARCH:-clang_64}" -O "${QT_INSTALL_DIR}"
fi
QT_CMAKE_DIR=$(find "${QT_INSTALL_DIR}/${QT_VERSION}" -maxdepth 1 -mindepth 1 -type d | head -n1)

echo "== Building OpenCV ${OPENCV_VERSION} from source (macOS ${MACOS_DEPLOYMENT_TARGET}+) =="
echo "   (only core/imgproc/imgcodecs - what this project actually uses; ~10-15 min)"
if [ -f "${OPENCV_INSTALL_DIR}/lib/cmake/opencv4/OpenCVConfig.cmake" ]; then
    echo "OpenCV already built at ${OPENCV_INSTALL_DIR}, skipping."
else
    SRC_DIR=$(mktemp -d)
    git clone --branch "${OPENCV_VERSION}" --depth 1 https://github.com/opencv/opencv.git "${SRC_DIR}"
    cmake -S "${SRC_DIR}" -B "${SRC_DIR}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOS_DEPLOYMENT_TARGET}" \
        -DCMAKE_INSTALL_PREFIX="${OPENCV_INSTALL_DIR}" \
        -DBUILD_LIST=core,imgproc,imgcodecs \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_PERF_TESTS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_opencv_apps=OFF \
        -DBUILD_DOCS=OFF
    cmake --build "${SRC_DIR}/build" -j "$(sysctl -n hw.ncpu)"
    cmake --install "${SRC_DIR}/build"
    rm -rf "${SRC_DIR}"
fi

cat <<EOF

Done. Configure and build the project with:

  cmake -B build -DCMAKE_PREFIX_PATH="${QT_CMAKE_DIR};${OPENCV_INSTALL_DIR}"
  cmake --build build -j

Then run it with: open build/PhotosByLarry.app
EOF
