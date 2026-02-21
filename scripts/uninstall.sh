#!/bin/bash

sudo systemctl stop lgsdsu.service
sudo systemctl disable lgsdsu.service
sudo rm /etc/systemd/system/lgsdsu.service
sudo systemctl daemon-reload
sudo rm /etc/modprobe.d/fix-iio.conf
sudo rm /etc/modules-load.d/fix-iio-sensor-hub.conf

sudo rm -rf /LegionGoSGyroDSU
