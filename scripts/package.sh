#!/bin/bash

scripts/build.sh Release

tar -czvf LegionGoGyroDSU.tar.gz \
    -C scripts check.sh lgdsu.service install.sh uninstall.sh fix-iio-sensor-hub.conf \
    -C ../build LegionGoGyro
