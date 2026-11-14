#!/usr/bin/env bash
# inkpage - a simple PDF editor for Linux
# Copyright (C) 2026 Ludovic Mercier
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Builds the Debian package of inkpage (Release build, installed in /usr).
# Blend2D and Clipper2, which Debian does not package, are put into the package
# with the private libraries of inkpage (/usr/lib/<triplet>/inkpage).
#
# Usage: scripts/build-deb.sh [DEPS_DIR] [BUILD_DIR]
#
# DEPS_DIR   (default: ../deps) the directory of scripts/build-deps.sh, its
#            install/ directory must contain Blend2D and Clipper2
# BUILD_DIR  (default: build-deb) the build tree, the package is written there
#
# Required Debian packages: the build dependencies listed in README.md, and
# dpkg-dev (dpkg-shlibdeps computes the dependencies of the package).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$(cd "${1:-$REPO/../deps}" && pwd)"
BUILD="${2:-$REPO/build-deb}"

if [ ! -d "$DEPS/install/lib/cmake/blend2d" ] || [ ! -d "$DEPS/install/lib/cmake/clipper2" ]; then
    echo "Blend2D and Clipper2 are not installed in $DEPS/install: run scripts/build-deps.sh first." >&2
    exit 1
fi

cmake -S "$REPO" -B "$BUILD" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_PREFIX_PATH="$DEPS/install" \
      -DPDF4QT_BUILD_TESTS=OFF \
      -DPDF4QT_ENABLE_OCR=OFF \
      -DINKPAGE_BUNDLE_DEPS=ON
cmake --build "$BUILD"
(cd "$BUILD" && cpack -G DEB)

echo "Package:"
ls -1 "$BUILD"/*.deb
