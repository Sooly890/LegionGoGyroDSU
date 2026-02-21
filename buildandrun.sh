#!/bin/bash

set -e

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

scp build/LegionGoSGyro deck@steamdeck:~/LegionGoSGyro
ssh deck@steamdeck -t "chmod +x ~/LegionGoSGyro/LegionGoSGyro; sudo ~/LegionGoSGyro/LegionGoSGyro"
