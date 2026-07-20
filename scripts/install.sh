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

# These files are optional, including the legacy fix-iio.conf path.
sudo rm -f /etc/modprobe.d/fix-iio.conf
sudo rm -f /etc/modules-load.d/fix-iio-sensor-hub.conf
sudo rm -rf /LegionGoSGyroDSU

echo "Installing LegionGoSGyroDSU..."

sudo mkdir -p /LegionGoSGyroDSU
cd /LegionGoSGyroDSU

sudo wget https://github.com/Sooly890/LegionGoSGyroDSU/releases/latest/download/LegionGoSGyroDSU.tar.gz

sudo tar -xzvf LegionGoSGyroDSU.tar.gz
sudo rm -f LegionGoSGyroDSU.tar.gz

# fix a mistake I made in packaging, probably will fix it later
if sudo test -f LegionGoSGyro; then
    sudo mv LegionGoSGyro LegionGoSGyroDSU
fi
sudo chmod +x LegionGoSGyroDSU

sudo cp lgsdsu.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now lgsdsu.service

sudo chmod +x check.sh uninstall.sh install.sh

sudo cp fix-iio-sensor-hub.conf /etc/modules-load.d/

echo "Installation complete. Please reboot your system."
