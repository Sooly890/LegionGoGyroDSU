#!/bin/bash
set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <build_type> (e.g. Release, Debug)"
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=$1
cmake --build build --config $1
