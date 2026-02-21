#!/bin/bash

sudo systemctl status lgsdsu.service

lines=$(ls -1 /sys/bus/iio/devices | grep iio:device | wc -l)

if [[ $lines -ge 3 ]]; then
echo "Success!"
else
echo "Error: Force shutdown, wait 10 min, turn on again."
fi
