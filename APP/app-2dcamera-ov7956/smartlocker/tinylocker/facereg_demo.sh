#!/bin/sh

# just for test purpose
echo "Enter facereg_sh: 20190125"

cd /opt/smartlocker
./facereg

echo "No more operation, A7 enter VLLS!"
sleep 1
echo mem > /sys/power/state
