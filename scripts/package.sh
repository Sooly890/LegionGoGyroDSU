#!/bin/bash

scripts/build.sh

tar -czvf LegionGoSGyroDSU.tar.gz \
    -C scripts check.sh lgsdsu.service start.sh install.sh uninstall.sh \
    -C ../build LegionGoSGyro
