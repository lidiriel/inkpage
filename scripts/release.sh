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

# Creates the git tag of a release of inkpage. The version of inkpage, of the
# application and of the Debian package is taken from this tag (see
# cmake/InkpageVersion.cmake): the tag vX.Y.Z is the release X.Y.Z, the commits
# after it are the snapshots X.Y.Z+gitN.gHASH.
#
# Usage: scripts/release.sh X.Y.Z
#
# Before, commit the description of the release:
#   - packaging/changelog: a new first entry "inkpage (X.Y.Z) unstable; ..."
#   - Desktop/io.github.inkpage.metainfo.xml: <release version="X.Y.Z" date="..."/>
# The tag is created on the current commit (HEAD) and is not pushed.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

fail() {
    echo "release: $*" >&2
    exit 1
}

[ $# -eq 1 ] || fail "usage: scripts/release.sh X.Y.Z"
VERSION="$1"
TAG="v$VERSION"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "the version must be X.Y.Z (numbers), not '$VERSION'"

git diff --quiet && git diff --cached --quiet || fail "the working tree has uncommitted changes"
! git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || fail "the tag $TAG already exists"

# The new version follows the previous release
PREVIOUS="$(git describe --tags --abbrev=0 --match 'v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null || true)"
if [ -n "$PREVIOUS" ] && command -v dpkg >/dev/null; then
    dpkg --compare-versions "$VERSION" gt "${PREVIOUS#v}" || fail "$VERSION is not newer than the previous release $PREVIOUS"
fi

# The release is described by the changelog of the package and by the metainfo
FIRST_LINE="$(head -n 1 packaging/changelog)"
[[ "$FIRST_LINE" =~ ^inkpage\ \(([^\)]+)\)\ ([a-z-]+)\; ]] || fail "unexpected first line of packaging/changelog: $FIRST_LINE"
[ "${BASH_REMATCH[1]}" = "$VERSION" ] || fail "the last entry of packaging/changelog is ${BASH_REMATCH[1]}, add the entry of $VERSION"
[ "${BASH_REMATCH[2]}" != "UNRELEASED" ] || fail "the entry of $VERSION in packaging/changelog is UNRELEASED (use unstable)"
grep -q "<release version=\"$VERSION\"" Desktop/io.github.inkpage.metainfo.xml \
    || fail "add <release version=\"$VERSION\" date=\"...\"/> to Desktop/io.github.inkpage.metainfo.xml"

git tag -a "$TAG" -m "inkpage $VERSION"
echo "Created the tag $TAG on $(git rev-parse --short HEAD)."
echo "Build the package:  scripts/build-deb.sh"
echo "Publish the tag:    git push origin $TAG"
