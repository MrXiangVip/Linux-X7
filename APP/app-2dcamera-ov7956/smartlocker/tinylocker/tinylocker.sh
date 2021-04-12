#!/bin/sh

# should execute on 7ulp power on
echo "Enter tinylocker.sh: 20190215"

cd /opt/smartlocker
./tinylocker

echo "Face Recognition done, A7 enter VLLS!"
echo mem > /sys/power/state
