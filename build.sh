#!/bin/bash

set -e

if [ ! -f "/usr/include/libudev.h" ]; then
    apt-get install libudev-dev
fi

if [ ! -d "host" ]; then
    echo "Run this script in the folder containing build.sh" >&2
    exit 1
fi

if [ ! -d "build" ]; then
    mkdir build
fi
cd build
cmake ../host
make
