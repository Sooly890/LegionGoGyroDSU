#!/bin/bash

echo There may be errors, they do not matter, do not worry

sudo systemctl stop lgsdsu.service
sudo systemctl disable lgsdsu.service
sudo rm /etc/systemd/system/lgsdsu.service
sudo systemctl daemon-reload

sudo rm -rf /LegionGoSGyroDSU

echo Errors do matter from here, please make sure you have a password, you can run the "passwd" command if not.
sleep 2

sudo mkdir -p /LegionGoSGyroDSU
cd /LegionGoSGyroDSU

sudo wget https://github.com/Sooly890/LegionGoSGyroDSU/releases/latest/download/LegionGoSGyroDSU.tar.gz || exit 1

sudo tar -xzvf  LegionGoSGyroDSU.tar.gz || exit 1
sudo rm LegionGoSGyroDSU.zip
sudo chmod +x LegionGoSGyroDSU || exit 1

sudo cp lgsdsu.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable lgsdsu.service
sudo systemctl start lgsdsu.service

sudo chmod +x check.sh start.sh uninstall.sh

echo Installation complete!
