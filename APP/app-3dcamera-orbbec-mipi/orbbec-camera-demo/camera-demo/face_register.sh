#!/bin/sh

# should execute on 7ulp power on
echo "Enter face_recgnize.sh: 20190301"

cd /opt/orbbec-camera
./orbbec_rs_reg_face 1

echo "No more operation, A7 enter VLLS!"
echo mem > /sys/power/state
