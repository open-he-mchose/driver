#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds the Linux AppImage of mchose-gui from a configured and built tree (used by the release workflow).
#   packaging/linux/make-appimage.sh BUILD_DIR VERSION [OUT_DIR]
# Needs qmake of Qt 6 (QMAKE=..., or qmake6 / qmake in PATH). Downloads two pinned linuxdeploy tools (checked by
# SHA-256) into $TOOLS_DIR (default BUILD_DIR/appimage-tools).
set -euo pipefail
build=$(realpath "$1")
version=$2
out=$(realpath "${3:-.}")
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
tools=${TOOLS_DIR:-$build/appimage-tools}
mkdir -p "$tools" "$out"

get() {
    local name=$1 url=$2 sum=$3
    if ! echo "$sum  $tools/$name" | sha256sum -c --status 2>/dev/null; then
        curl -fsSL -o "$tools/$name" "$url"
        echo "$sum  $tools/$name" | sha256sum -c --quiet
    fi
    chmod +x "$tools/$name"
}
get linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
get linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

if [ -z "${QMAKE:-}" ]; then
    QMAKE=$(command -v qmake6 || command -v qmake || true)
fi
[ -n "$QMAKE" ] || { echo "qmake of Qt 6 not found (set QMAKE)"; exit 1; }
export QMAKE
plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
extra=""
for p in libqwayland-egl.so libqwayland-generic.so libqwayland.so libqoffscreen.so; do
    if [ -f "$plugins/platforms/$p" ]; then extra="$extra${extra:+;}$p"; fi
done
export EXTRA_PLATFORM_PLUGINS=$extra
case "$extra" in *wayland*) export EXTRA_QT_MODULES="waylandcompositor" ;; esac
export QML_SOURCES_PATHS=$root/gui/qml
export APPIMAGE_EXTRACT_AND_RUN=1          # works without FUSE (CI containers)
export LINUXDEPLOY_OUTPUT_VERSION=$version
export LDAI_OUTPUT=$out/mchose-gui-$version-x86_64.AppImage
export OUTPUT=$LDAI_OUTPUT

appdir=$build/AppDir
rm -rf "$appdir"
cd "$build"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    -e "$build/gui/mchose-gui" -d "$here/mchose-gui.desktop" -i "$here/mchose-gui.svg" \
    --plugin qt --output appimage
echo "built $LDAI_OUTPUT"
