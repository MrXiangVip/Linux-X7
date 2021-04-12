#!/bin/sh

USB_SYS_DIR="/sys/bus/usb/devices"
USB_DEV_DIR="/dev/bus/usb"
ORBBEC_VID="2bc5"
ORBBEC_ASTRAUVC_PID="050b"
ORBBEC_DEEYEA_PID="060b"
ORBBEC_ASTRAUVC_DEVNAME="/dev/astrauvc"
ORBBEC_DEEYEA_DEVNAME="/dev/deeyea"

for each_item in $(ls ${USB_SYS_DIR}/*/idVendor); do
	vid_value=$(cat $each_item)
	if [ x"$vid_value" == x"$ORBBEC_VID" ]; then
		# Found an orbbec device
		device_path=$(dirname $each_item)
		pid_value=$(cat ${device_path}/idProduct)
		dev_num=$(cat ${device_path}/devnum)
		#dev_path=$(cat ${device_path}/devpath)
		bus_num=$(cat ${device_path}/busnum)
		case $pid_value in
		${ORBBEC_ASTRAUVC_PID})
			ln -s ${USB_DEV_DIR}/00${bus_num}/${dev_num} ${ORBBEC_ASTRAUVC_DEVNAME}
			;;
		${ORBBEC_DEEYEA_PID})
			ln -s ${USB_DEV_DIR}/00${bus_num}/${dev_num} ${ORBBEC_DEEYEA_DEVNAME}
			;;
		*)
			echo "Product ID ${pid_value} not supported yet!"
		esac
	fi
done
