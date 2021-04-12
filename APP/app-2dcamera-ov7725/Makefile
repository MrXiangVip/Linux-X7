# file      Makefile
# author    $Author$
# version   $Rev$
# date      $Date$
# brief     a simple makefile to (cross) compile.
#           uses the openembedded provided sysroot and toolchain
# target    Linux on Apalis/Colibri modules
# caveats   -

CC ?= gcc
CXX ?= g++
RM = rm -f
STRIP ?= strip

FACE_PROG = face_loop
RECG_PROG = face_recognize
REG_PROG = face_register
PFMGMT_PROG = pfmgr_main
VERSION = 0.1
PREFIX = /usr/local/bin
ALL_DEPS = *.o.d

WARNINGS = -Wall
HOST_ROOTFS ?= $(OECORE_TARGET_SYSROOT)
ifeq ($(DEBUG), 1)
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O0 -g $(WARNINGS) -I$(HOST_ROOTFS) -I$(HOST_ROOTFS/usr/include) -I./Common -I./libsimplelog/inc -I./ -I./FIT-0.2/include -I./mqttsrc
else
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 $(WARNINGS) -I$(HOST_ROOTFS) -I$(HOST_ROOTFS/usr/include) -I./Common -I./libsimplelog/inc -I./ -I./FIT-0.2/include -I./mqttsrc
endif

COMM_SRCS += uart_handle.cpp
COMM_SRCS += hl_MsgQueue.cpp
COMM_SRCS += sys_upgrade.cpp
COMM_SRCS += per_calc.cpp
COMM_SRCS += sig_timer.cpp
COMM_SRCS += util_crc32.cpp
COMM_SRCS += util_crc16.cpp
COMM_SRCS += function.cpp
COMM_SRCS += FitApiHandle.cpp
COMM_SRCS += pfmgr.cpp
COMM_SRCS += bmpTransMat.cpp
COMM_SRCS += mqttsrc/configV2.cpp
COMM_SRCS += mqttsrc/config.cpp
COMM_SRCS += mqttsrc/mqttutil.cpp

RECG_SRCS = face_recgnize.cpp
RECG_SRCS += $(COMM_SRCS)
REG_SRCS = face_reg.cpp
REG_SRCS += $(COMM_SRCS)
FACE_SRCS = face_loop.cpp
FACE_SRCS += $(COMM_SRCS)
PFMGMT_SRCS = pfmgr_main.cpp
PFMGMT_SRCS += $(COMM_SRCS)

# Sets the output filename and object files
RECG_OBJS= $(RECG_SRCS:.cpp=.o)
RECG_DEPS= $(RECG_OBJS:.o=.d)

REG_OBJS= $(REG_SRCS:.cpp=.o)
REG_DEPS= $(REG_OBJS:.o=.d)

FACE_OBJS= $(FACE_SRCS:.cpp=.o)
FACE_DEPS= $(FACE_OBJS:.o=.d)

PFMGMT_OBJS= $(PFMGMT_SRCS:.cpp=.o)
PFMGMT_DEPS= $(PFMGMT_OBJS:.o=.d)

LIB_SIMPLELOG = -L./libsimplelog -lsimplelog -I./libsimplelog/inc
LIB_YUV = -L./libyuv -lyuv -I./include
LIB_FIT = -L./FIT-0.2/lib -lfit -I./FIT-0.2/include
LIBS = -lpthread -lm -L$(HOST_ROOTFS)/usr/lib $(LIB_SIMPLELOG) $(LIB_YUV)
LIBS_LITE = -lpthread -lm -L$(HOST_ROOTFS)/usr/lib $(LIB_SIMPLELOG) $(LIB_YUV)
 

# pull in dependency info for *existing* .o files
-include $(DEPS)

all: mqttbin liblog $(RECG_PROG) $(REG_PROG) $(FACE_PROG) $(PFMGMT_PROG)

$(RECG_PROG): $(RECG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(RECG_OBJS) $(LIBS) $(LDFLAGS) $(LIB_FIT)
	#$(STRIP) $@

$(REG_PROG): $(REG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(REG_OBJS) $(LIBS) $(LDFLAGS) $(LIB_FIT)
	#$(STRIP) $@

$(FACE_PROG): $(FACE_OBJS) 
	$(CXX) $(CFLAGS) -o $@ $(FACE_OBJS) $(LIBS) $(LDFLAGS) $(LIB_FIT)
	#$(STRIP) $@

$(PFMGMT_PROG): $(PFMGMT_OBJS) 
	$(CXX) $(CFLAGS) -o $@ $(PFMGMT_OBJS) $(LIBS) $(LDFLAGS) $(LIB_FIT)
	#$(STRIP) $@

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<
	$(CXX) -MM $(CFLAGS) $< > $@.d

liblog:
	make -C ./libsimplelog
mqttbin:
	make -C mqttsrc
	rm ./mqtt
	cp ./mqttsrc/mqtt ./
cleanlibs:
	@echo "Cleaning up libraries."
	make -C ./libsimplelog clean
	
.PHONY: clean
clean:
	@echo "Cleaning up directory."
	@echo $(ALL_DEPS)
	$(RM) $(RECG_OBJS) $(REG_OBJS) $(FACE_OBJS) $(PFMGMT_OBJS) $(PMGMT_OBJS) $(RECG_PROG) $(REG_PROG) $(FACE_PROG) $(PFMGMT_PROG) $(ALL_DEPS)
	make -C ./libsimplelog clean
	make -C ./mqttsrc clean
install:
	install $(RECG_PROG) $(PREFIX)
	install $(REG_PROG) $(PREFIX)
	install $(FACE_PROG) $(PREFIX)
	install $(PFMGMT_PROG) $(PREFIX)

