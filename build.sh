#!/usr/bin/env bash

# Build script for x11 desktop shell
# Use:
# <empty>: build normally
# setup: install all dependencies
# test: build and run the unit tests
# install: build and deploy into /usr/bin/
# run: kill the running shell, install and start it

set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

cmd_setup() {
    sudo pacman -S --needed meson ninja gcc clang pkgconf libx11 libxcb xcb-util-wm xcb-util-keysyms cairo pango fontconfig resvg stb sdbus-cpp bluez networkmanager upower libxkbcommon-x11 polkit fd
}

cmd_build() {
    [[ -f build/build.ninja ]] || meson setup build --prefix=/usr
    meson compile -C build
}

cmd_test() { cmd_build; meson test -C build --print-errorlogs; }
cmd_install() { cmd_build; sudo meson install -C build --no-rebuild; }

cmd_run() { astralia-shell kill 2>/dev/null || true; cmd_install; astralia-shell; }

main() {
    local cmd="${1:-build}"
    case "$cmd" in
    setup | build | test | install | run) "cmd_$cmd" ;;
    *) echo "unknown command: $cmd" >&2; exit 2 ;;
    esac
}

main "$@"
