#!/bin/bash

sudo systemctl stop lgsdsu.service
sudo systemctl disable lgsdsu.service
sudo rm -f /etc/systemd/system/lgsdsu.service

sudo systemctl stop lgdsu.service
sudo systemctl disable lgdsu.service
sudo rm -f /etc/systemd/system/lgdsu.service

sudo systemctl daemon-reload

sudo rm -f /etc/modprobe.d/fix-iio.conf
sudo rm -f /etc/modules-load.d/fix-iio-sensor-hub.conf

sudo rm -rf /LegionGoSGyroDSU
sudo rm -rf /LegionGoGyroDSU
