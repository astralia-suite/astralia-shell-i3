#!/usr/bin/env bash

# Build script for x11 desktop shell
# Use:
# <empty>: build normally
# setup: install all dependencies
# test: build and run the unit tests
# install: build and deploy into /usr/bin/
# run: kill the running shell, install and start it
# NATIVE_CPU=false: build without the X201 tuning (-march=westmere)

set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

cmd_setup() {
    sudo pacman -S --needed meson ninja gcc clang pkgconf libxcb xcb-util-wm xcb-util-keysyms cairo pango fontconfig resvg libjpeg-turbo stb sdbus-cpp libxkbcommon libxkbcommon-x11 polkit libpipewire pipewire wireplumber bluez networkmanager upower fd brightnessctl xorg-server-xephyr
}

cmd_build() {
    local native="-Dnative_cpu=${NATIVE_CPU:-true}"
    if [[ -f build/build.ninja ]]; then
        meson configure build "$native"
    else
        meson setup build --prefix=/usr "$native"
    fi
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
