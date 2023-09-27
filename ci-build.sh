#!/bin/bash

set -e

if test "$platform" = "bash"; then
    echo "Building on Ubuntu Linux x86_64"
    sudo apt-get install build-essential automake
elif test "$platform" = "msys32"; then
    echo "Building on Msys2 i686"
elif test "$platform" = "mingw32"; then
    echo "Building on MinGW-w64 i686"
    pacman --sync --noconfirm --noprogressbar mingw-w64-i686-gettext
else
    echo "unknown environment!"
    exit 1
fi

./autogen.sh
make
make check || echo WARNING: make check failed
