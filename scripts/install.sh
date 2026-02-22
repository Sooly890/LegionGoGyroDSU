#!/bin/bash

echo There may be errors, they do not matter, do not worry

sudo systemctl stop lgsdsu.service
sudo systemctl disable lgsdsu.service
sudo rm /etc/systemd/system/lgsdsu.service
sudo systemctl daemon-reload
sudo rm /etc/modprobe.d/fix-iio.conf # old, but in case anybody installed it from the era
sudo rm /etc/modules-load.d/fix-iio-sensor-hub.conf

sudo rm -rf /LegionGoSGyroDSU

echo Errors do matter from here, please make sure you have a password, you can run the "passwd" command if not.
sleep 2

sudo mkdir -p /LegionGoSGyroDSU
cd /LegionGoSGyroDSU

sudo wget https://github.com/Sooly890/LegionGoSGyroDSU/releases/latest/download/LegionGoSGyroDSU.tar.gz || exit 1

sudo tar -xzvf  LegionGoSGyroDSU.tar.gz || exit 1
sudo rm LegionGoSGyroDSU.tar.gz

# fix a mistake I made in packaging, probably will fix it later
sudo mv LegionGoSGyro LegionGoSGyroDSU > /dev/null 2>&1
sudo chmod +x LegionGoSGyroDSU || exit 1

sudo cp lgsdsu.service /etc/systemd/system/ || exit 1

sudo systemctl daemon-reload || exit 1
sudo systemctl enable lgsdsu.service || exit 1
sudo systemctl start lgsdsu.service

sudo chmod +x check.sh start.sh uninstall.sh install.sh || exit 1

sudo cp fix-iio-sensor-hub.conf /etc/modules-load.d/ || exit 1

echo Installation complete, please reboot your system!
