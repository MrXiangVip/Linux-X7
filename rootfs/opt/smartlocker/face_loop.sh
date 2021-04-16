#!/bin/sh

cd /opt/smartlocker
./face_loop &
if [ ! -f "/opt/smartlocker/config.ini" ]
	then
	cp /opt/smartlocker/config.ini-bak /opt/smartlocker/config.ini
	chmod 777 *
	(./init.sh &)
fi
./mqtt &
