#!/bin/bash

sudo systemctl stop lgsdsu.service
sudo systemctl disable lgsdsu.service
sudo rm /etc/systemd/system/lgsdsu.service
sudo systemctl daemon-reload

sudo rm -rf /LegionGoSGyroDSU
