#!/bin/bash

scripts/build.sh

tar -czvf LegionGoSGyroDSU.tar.gz \
    -C scripts check.sh lgsdsu.service install.sh uninstall.sh fix-iio-sensor-hub.conf \
    -C ../build LegionGoSGyro
