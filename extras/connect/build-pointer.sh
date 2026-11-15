#!/bin/sh
set -eu
connect_source=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
connect_build=$(mktemp -d)
trap 'rm -rf "$connect_build"' EXIT HUP INT TERM
wayland-scanner client-header "$connect_source/../../protocols/wlr-virtual-pointer-unstable-v1.xml" "$connect_build/virtual-pointer-client.h"
wayland-scanner private-code "$connect_source/../../protocols/wlr-virtual-pointer-unstable-v1.xml" "$connect_build/virtual-pointer-protocol.c"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$connect_build" "$connect_source/pointer.c" "$connect_build/virtual-pointer-protocol.c" $(pkg-config --cflags --libs wayland-client) -o "$connect_build/pointer"
install -m755 "$connect_build/pointer" "$1"
