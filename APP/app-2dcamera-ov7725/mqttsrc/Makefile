MQTT = mqtt

MQTT_SRC = mqtt.cpp config.c iniparser.c dictionary.c uart-handle.c util.cpp mqtt-api.c cJSON.c base64.c mqtt-mq.c log.c configV2.c mqttutil.cpp


CC = gcc -std=c89


.PHONY: all clean install

all: mqtt

$(MQTT): $(MQTT_SRC) 
	arm-poky-linux-gnueabi-g++  -march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi -DLINUX -DVERSION=\"0.1\" -O0 -Wall -I/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi  -o $(MQTT) $(MQTT_SRC) -lpthread -lm -L/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi/usr/lib -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed
	#arm-poky-linux-gnueabi-g++  -march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi -DLINUX -DVERSION=\"0.1\" -O0 -g -Wall -I/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi  -o $(MQTT) $(MQTT_SRC) -lpthread -lm -L/opt/fsl-imx-wayland/4.9.88-2.0.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi/usr/lib -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed

clean:
	rm -rf $(MQTT)
