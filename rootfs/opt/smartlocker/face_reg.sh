#!/bin/sh

# should execute on 7ulp power on
echo "Enter face_reg.sh: 20200723"

cd /opt/smartlocker
./face_register

#echo "No more operation, A7 enter VLLS!"
#echo mem > /sys/power/state
