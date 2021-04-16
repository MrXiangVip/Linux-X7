#!/bin/sh

# Items that might need to be updated are:
# 1. APP, face_recognize
# 2. APP, face_register
# 2. zImage
# 4. sdk20-app.img

SCRIPT_PATH=$(dirname $0)
BOOT_PART_DEV="/dev/mmcblk0p1"
BOOT_PART_MOUNT_PATH="/home/root/boot"
APP_PATH="/opt/smartlocker"
FACE_LIB_PATH="/usr/lib"
APP_FACE_RECOGNIZE_NAME="face_recognize"
APP_FACE_REGISTER_NAME="face_register"
KERNEL_IMAGE_NAME="zImage"
DTB_NAME="imx7ulp-lock.dtb"
M4_IMAGE_NAME="sdk20-app.img"
FACE_LIB_NAME="libReadFace.so"
CHKSUM_EXT=".md5"
TEMP_EXT=".tmp"

RED_COLOR='\E[1;31m'
GREEN_COLOR='\E[1;32m'
YELOW_COLOR='\E[1;33m'
BLUE_COLOR='\E[1;34m'
PINK_COLOR='\E[1;35m'
RES='\E[0m'

UPDATE_ITEMS="
$M4_IMAGE_NAME
"

updater_print_done()
{
	echo -e "${GREEN_COLOR}DONE${RES}\n"
}

updater_print_fail()
{

	echo -e "${RED_COLOR}FAIL${RES}\n"
}

updater_print_ok()
{
	echo -e "${GREEN_COLOR}OK${RES}"
}

updater_mount_boot_part()
{
	echo "+---------------------------------------------"
	echo "+ Mount boot partition"
	echo "+---------------------------------------------"
	if [ ! -d $BOOT_PART_MOUNT_PATH ]; then
		echo "boot directory not found, create a boot directory"
		mkdir -p $BOOT_PART_MOUNT_PATH
	fi

	if [ ! -e $BOOT_PART_DEV ]; then
		echo "boot partition device not found, exit!"
		return 1
	fi
	echo "Mount partition ... ..."
	mount -t vfat $BOOT_PART_DEV $BOOT_PART_MOUNT_PATH
	if [ $? == 0 ]; then
		updater_print_done
		return 1
	else
		updater_print_fail
		return 0
	fi
}

# $1 - source file
# $2 - destine file
updater_update_file()
{
	rm -f ${2}${TEMP_EXT}
	cp -f $1 ${2}${TEMP_EXT}
	rm -f $2
	mv ${2}${TEMP_EXT} ${2}
	sync
}

updater_update_register_app()
{
	echo "+---------------------------------------------"
	echo "+ Update Face Register APP "
	echo "+---------------------------------------------"
	if [ ! -d $APP_PATH ]; then
		echo "$APP_PATH not found, exit!"
		return 1
	fi
	if [ ! -f $APP_PATH/$APP_FACE_REGISTER_NAME ]; then
		echo "$APP_PATH/$APP_FACE_REGISTER_NAME not found, exit!"
		return 1
	fi

	echo -n "Copying $APP_FACE_REGISTER_NAME ... ..."
	updater_update_file $SCRIPT_PATH/$APP_FACE_REGISTER_NAME $APP_PATH/${APP_FACE_REGISTER_NAME}
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi

}

updater_update_recognize_app()
{
	echo "+---------------------------------------------"
	echo "+ Update Face Recognize APP "
	echo "+---------------------------------------------"
	if [ ! -d $APP_PATH ]; then
		echo "$APP_PATH not found, exit!"
		return 1
	fi
	if [ ! -f $APP_PATH/$APP_FACE_RECOGNIZE_NAME ]; then
		echo "$APP_PATH/$APP_FACE_RECOGNIZE_NAME not found, exit!"
		return 1
	fi

	echo -n "Copying $APP_FACE_RECOGNIZE_NAME ... ..."
	updater_update_file $SCRIPT_PATH/$APP_FACE_RECOGNIZE_NAME $APP_PATH/$APP_FACE_RECOGNIZE_NAME
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi
}

updater_update_zImage()
{
	echo "+---------------------------------------------"
	echo "+ Update $KERNEL_IMAGE_NAME"
	echo "+---------------------------------------------"
	if [ ! -f $BOOT_PART_MOUNT_PATH/$KERNEL_IMAGE_NAME ]; then
		echo "$BOOT_PART_MOUNT_PATH/$KERNEL_IMAGE_NAME not found, exit!"
		return 1
	fi

	echo -n "Copying $KERNEL_IMAGE_NAME ... ..."
	updater_update_file $SCRIPT_PATH/$KERNEL_IMAGE_NAME $BOOT_PART_MOUNT_PATH/$KERNEL_IMAGE_NAME
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi
}

updater_update_face_lib()
{
	echo "+---------------------------------------------"
	echo "+ Update $FACE_LIB_NAME"
	echo "+---------------------------------------------"
	if [ ! -f ${FACE_LIB_PATH}/${FACE_LIB_NAME} ]; then
		echo "${FACE_LIB_PATH}/${FACE_LIB_NAME} not found, exit!"
		return 1
	fi

	echo -n "Copying ${FACE_LIB_NAME} ... ..."
	updater_update_file $SCRIPT_PATH/${FACE_LIB_NAME} ${FACE_LIB_PATH}/${FACE_LIB_NAME}
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi

}

updater_update_dtb()
{
	echo "+---------------------------------------------"
	echo "+ Update $DTB_NAME "
	echo "+---------------------------------------------"
	if [ ! -f $BOOT_PART_MOUNT_PATH/$DTB_NAME ]; then
		echo "$BOOT_PART_MOUNT_PATH/$DTB_NAME not found, exit!"
		return 1
	fi

	echo -n "Copying $DTB_NAME ... ..."
	updater_update_file $SCRIPT_PATH/$DTB_NAME $BOOT_PART_MOUNT_PATH/$DTB_NAME
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi
}

updater_update_m4_image()
{
	echo "+---------------------------------------------"
	echo "+ Update $M4_IMAGE_NAME "
	echo "+---------------------------------------------"
	if [ ! -f $BOOT_PART_MOUNT_PATH/$M4_IMAGE_NAME ]; then
		echo "$BOOT_PART_MOUNT_PATH/$M4_IMAGE_NAME not found, exit!"
		return 1
	fi

	echo -n "Copying $M4_IMAGE_NAME ... ..."
	updater_update_file $SCRIPT_PATH/$M4_IMAGE_NAME $BOOT_PART_MOUNT_PATH/$M4_IMAGE_NAME
	if [ $? == 0 ]; then
		updater_print_done
		return 0
	else
		updater_print_fail
		return 1
	fi
}

updater_clean()
{
	echo "+---------------------------------------------"
	echo "+ Clean "
	echo "+---------------------------------------------"

	if [ -d $BOOT_PART_MOUNT_PATH ]; then
		echo -n "Unmount boot partitino ... ..."
		umount $BOOT_PART_MOUNT_PATH
		if [ $? == 0 ]; then
			updater_print_done
			return 0
		else
			updater_print_fail
			return 1
		fi
	fi
}

updater_checksum_all()
{
	# Checksum
	echo "+---------------------------------------------"
	echo "+ Doing checksum"
	echo "+---------------------------------------------"
	cd $SCRIPT_PATH
	for each_file in $UPDATE_ITEMS; do
		echo -n "${each_file}: "
		if [ ! -f ./${each_file} ]; then
			updater_print_fail
			echo "File not found: $SCRIPT_PATH/$each_file!"
			return 1
		fi
		if [ ! -f ./${each_file}${CHKSUM_EXT} ]; then
			updater_print_fail
			echo "Checksum file not found: $SCRIPT_PATH/${each_file}${CHKSUM_EXT}!"
			return 1
		fi
		if md5sum -c ./${each_file}${CHKSUM_EXT}; then
			updater_print_ok
		else
			updater_print_fail
			cd $OLDPWD
			return 1
		fi
	done
	cd $OLDPWD
}

updater_update_all()
{
	rtn_rslt=0
	# Mount boot partition
	updater_mount_boot_part

	# Update image
	for each_file in $UPDATE_ITEMS; do
		case $each_file in
			$KERNEL_IMAGE_NAME )
				# zImage
				updater_update_zImage
				rtn_rslt=$?
				;;
			$M4_IMAGE_NAME )
				# sdk20-app.img
				updater_update_m4_image
				rtn_rslt=$?
				;;
			$APP_FACE_RECOGNIZE_NAME )
				# APP, face_recognize 
				updater_update_recognize_app
				rtn_rslt=$?
				;;
			$APP_FACE_REGISTER_NAME )
				# APP, face_register
				updater_update_register_app
				rtn_rslt=$?
				;;
			$FACE_LIB_NAME )
				# Face library
				updater_update_face_lib
				rtn_rslt=$?
				;;
			$DTB_NAME )
				# dtb, may not need
				updater_update_dtb
				rtn_rslt=$?
				;;
		esac
	done

	# Clean
	updater_clean

	return $rtn_rslt
}

echo "+=============================================="
echo " ####         ######      ##### ##########     "
echo " ######      # ######    ##### #############   "  
echo " #######     ## ######  ##### ###############  "   
echo " ########    ### ########### ####       #####  "   
echo " #### #####  #### ######### #####       #####  "   
echo " ####  ##### #### ######### #################  "   
echo " ####   ######## ########### ###############   "   
echo " ####     ##### ######  ##### ############     "   
echo " ####      ### ######    ##### ##              "   
echo " ####       # ######      ##### #              "
echo "+=============================================="

echo "+=============================================="
echo "+             OTA Updater                      "
echo "+=============================================="

# Validate all updated files
updater_checksum_all
if [ $? != 0 ]; then
	echo "+=============================================="
	echo "+             Checksum FAILED!                 "
	echo "+=============================================="
	exit 1
fi

# Update all files
updater_update_all
if [ $? == 0 ]; then
	echo "+=============================================="
	echo "+             Update Succeed!"
	echo "+=============================================="
else
	echo "+=============================================="
	echo "+             Update Failed!"
	echo "+=============================================="
fi

#reboot


