#!/bin/sh

# should execute on 7ulp power on
echo "Enter facereg.sh: 20190215"

cd /opt/smartlocker
./facereg

echo "Face Recognition done, A7 enter VLLS!"
echo mem > /sys/power/state
