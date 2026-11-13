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

# Builds the inkpage dependencies that Debian does not package
# (AsmJit -> Blend2D, Clipper2), at the commits used by PDF4QT 1.7.0.0.
#
# Usage: scripts/build-deps.sh [DEPS_DIR]
#
# DEPS_DIR (default: ../deps next to the inkpage repository) receives:
#   src/      the sources, cloned from GitHub if missing
#   build/    the build trees (can be deleted afterwards)
#   install/  the libraries: pass it to CMAKE_PREFIX_PATH and
#             CMAKE_BUILD_RPATH (<DEPS_DIR>/install/lib) when configuring inkpage
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${1:-$REPO/../deps}"
mkdir -p "$DEPS"
DEPS="$(cd "$DEPS" && pwd)"
SRC="$DEPS/src"
BUILD="$DEPS/build"
PREFIX="$DEPS/install"

# name, repository, commit
fetch() {
    local name="$1" url="$2" commit="$3"
    if [ ! -d "$SRC/$name/.git" ]; then
        git clone "$url" "$SRC/$name"
    fi
    if [ "$(git -C "$SRC/$name" rev-parse HEAD)" != "$commit" ]; then
        git -C "$SRC/$name" fetch origin
        git -C "$SRC/$name" checkout --detach "$commit"
    fi
}

mkdir -p "$SRC"
fetch asmjit   https://github.com/asmjit/asmjit.git         c87860217e43e2a06060fcaae5b468f6a55b9963
fetch blend2d  https://github.com/blend2d/blend2d.git       6dbc2cefbc996379e07104e34519a440b49b15d7
fetch Clipper2 https://github.com/AngusJohnson/Clipper2.git 21ebba05db8894f0c7217ad35ea518080f324946

cmake -S "$SRC/blend2d" -B "$BUILD/blend2d" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
      -DBLEND2D_STATIC=OFF -DBLEND2D_NO_FUTEX=OFF -DASMJIT_DIR="$SRC/asmjit"
cmake --build "$BUILD/blend2d"
cmake --install "$BUILD/blend2d"

cmake -S "$SRC/Clipper2/CPP" -B "$BUILD/clipper2" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
      -DCLIPPER2_EXAMPLES=OFF -DCLIPPER2_TESTS=OFF -DCLIPPER2_UTILS=ON \
      -DBUILD_SHARED_LIBS=ON
cmake --build "$BUILD/clipper2"
cmake --install "$BUILD/clipper2"

echo "Done. Installed into $PREFIX"
