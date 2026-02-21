#!/bin/bash

lines=$(ls -1 /sys/bus/iio/devices | grep iio:device | wc -l)

if [[ $lines -ge 3 ]]; then
echo "Success!"
else
echo "Error: Force reboot, wait 10 min, restart."
fi
