#!/bin/bash
set -euo pipefail

# Unlock the SteamOS filesystem when that command is available. It returns an
# error on some systems when the filesystem is already writable, which is not
# an installation failure.
if command -v steamos-readonly >/dev/null 2>&1; then
    sudo steamos-readonly disable >/dev/null 2>&1 || true
fi

echo "Cleaning up a previous installation, if present..."

# Only contact systemd when this installer previously installed a unit. This
# keeps a first installation quiet while still stopping upgrades cleanly.
if sudo test -f /etc/systemd/system/lgsdsu.service; then
    sudo systemctl disable --now lgsdsu.service >/dev/null 2>&1 || true
    sudo rm -f /etc/systemd/system/lgsdsu.service
    sudo systemctl daemon-reload
fi

if sudo test -f /etc/systemd/system/lgdsu.service; then
    sudo systemctl disable --now lgdsu.service >/dev/null 2>&1 || true
    sudo rm -f /etc/systemd/system/lgdsu.service
    sudo systemctl daemon-reload
fi

# These files are optional, including the legacy fix-iio.conf path.
sudo rm -f /etc/modprobe.d/fix-iio.conf
sudo rm -f /etc/modules-load.d/fix-iio-sensor-hub.conf
sudo rm -rf /LegionGoSGyroDSU
sudo rm -rf /LegionGoGyroDSU

echo "Installing LegionGoGyroDSU..."

sudo mkdir -p /LegionGoGyroDSU
cd /LegionGoGyroDSU

#sudo wget https://github.com/Sooly890/LegionGoSGyroDSU/releases/latest/download/LegionGoSGyroDSU.tar.gz
sudo wget https://github.com/Sooly890/LegionGoGyroDSU/releases/latest/download/LegionGoGyroDSU.tar.gz

sudo tar -xzvf LegionGoGyroDSU.tar.gz
sudo rm -f LegionGoGyroDSU.tar.gz

# fix a mistake I made in packaging, probably will fix it later
if sudo test -f LegionGoSGyro; then
    sudo mv LegionGoSGyro LegionGoSGyroDSU
fi
if sudo test -f LegionGoGyro; then
    sudo mv LegionGoGyro LegionGoGyroDSU
fi
sudo chmod +x LegionGoGyroDSU

sudo cp lgdsu.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now lgdsu.service

sudo chmod +x check.sh uninstall.sh install.sh

sudo cp fix-iio-sensor-hub.conf /etc/modules-load.d/

echo "Installation complete. Please reboot your system."
