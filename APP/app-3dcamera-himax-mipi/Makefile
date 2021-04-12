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

LOOP_PROG = face_loop
RECG_PROG = face_recognize
REG_PROG = face_register
VERSION = 0.1
PREFIX = /opt/local/bin
LIB_INSTALL_DIR = /usr/lib

RS_VER=v2

DB_SRCS = face_db.cpp
HIMAX_SRCS = himax/cam_ctrl.cpp
OV7956_SRCS = ov7956/cam_ctrl.cpp
ORBBEC_SRCS = orbbec_mipi/cam_ctrl.cpp
TVIN_SRCS = libFaceNeuralNetwork/face_alg.cpp $(DB_SRCS)
RS_SRCS = libRS$(RS_VER)/face_alg.cpp
HIMAX_QH430MR_SRCS = himax_qh430mr/cam_ctrl.cpp

COMM_SRCS += sys_upgrade.cpp
COMM_SRCS += himax_qh430mr/projector_ctrl.cpp 
COMM_SRCS += util_crc32.cpp 
COMM_SRCS += function.cpp 
COMM_SRCS += sig_timer.cpp 
COMM_SRCS += fb_ctrl.cpp 
COMM_SRCS += per_calc.cpp
COMM_SRCS += img_dbg.cpp
COMM_SRCS += $(HIMAX_QH430MR_SRCS)
COMM_SRCS += $(RS_SRCS)
COMM_SRCS += banner/bmp_banner.cpp
LOOP_SRCS = faceloop.cpp
LOOP_SRCS += $(COMM_SRCS)
RECG_SRCS = face_recognize.cpp
RECG_SRCS += $(COMM_SRCS)
REG_SRCS  = face_register.cpp
REG_SRCS  += $(COMM_SRCS)
ALL_SRCS += $(COMM_SRCS) face_recognize.cpp face_register.cpp

# Sets the output filename and object files
LOOP_OBJS = $(LOOP_SRCS:.cpp=.o)
RECG_OBJS = $(RECG_SRCS:.cpp=.o)
REG_OBJS  = $(REG_SRCS:.cpp=.o)
ALL_OBJS = $(ALL_SRCS:.cpp=.o)
ALL_DEPS = $(ALL_OBJS:.o=.o.d)


HDR_HIMAX = -I./himax -I./himax/libuvc/inc
HDR_HIMAX_QH430MR = -I./himax_qh430mr -I./himax_qh430mr/libhxpp
HDR_TVIN = -I./libFaceNeuralNetwork
HDR_RS = -I./libRS$(RS_VER) -I./libRS$(RS_VER)/inc
HDRS = -I./Common -I./inc -I./banner
HDRS += $(HDR_HIMAX_QH430MR) $(HDR_RS)

LIB_SQLITE3 = -lsqlite3
LIB_OPENCV = -lopencv_imgproc -lopencv_highgui -lopencv_core
LIB_YUV = -L./libyuv -lyuv -ljpeg -I./libyuv/inc
LIB_TVIN = -L./libFaceNeuralNetwork -lFaceNeuralNetwork -I./libFaceNeuralNetwork $(LIB_OPENCV) $(LIB_SQLITE3)
LIB_ORBBEC = -L./orbbec_mipi/libodepth -lodepth -I./orbbec_mipi/libodepth
LIB_HIMAX = -L./himax/libuvc -luvc
LIB_HIMAX_QH430MR = -L./himax_qh430mr/libhxpp -lhxpp
LIB_SIMPLELOG = -L./libsimplelog -lsimplelog -I./libsimplelog/inc
LIB_RS = -L./libRS$(RS_VER) -lReadFace -I./libRS$(RS_VER)/inc
LIBS = -lpthread -lm -L$(HOST_ROOTFS)/usr/lib $(LIB_SIMPLELOG) $(LIB_YUV) $(LIB_HIMAX_QH430MR) $(LIB_RS)

WARNINGS = -Wall
HOST_ROOTFS ?= $(OECORE_TARGET_SYSROOT)
ifeq ($(DEBUG), 1)
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 -g $(WARNINGS) -I$(HOST_ROOTFS) -I$(HOST_ROOTFS/usr/include) -I$(HDRS)
else
	#CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O0 -g $(WARNINGS) -I$(HOST_ROOTFS) -I$(HDRS)
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 $(WARNINGS) -I$(HOST_ROOTFS) -I$(HDRS)
endif

all: $(RECG_PROG) $(REG_PROG) $(LOOP_PROG)

libs:
	make -C ./libsimplelog

$(LOOP_PROG): $(LOOP_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(LOOP_OBJS) $(LIBS) $(LDFLAGS) -Wl,-Map=$@.map
	$(STRIP) $@
	
$(RECG_PROG): $(RECG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(RECG_OBJS) $(LIBS) $(LDFLAGS) -Wl,-Map=$@.map
	$(STRIP) $@

$(REG_PROG): $(REG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(REG_OBJS) $(LIBS) $(LDFLAGS) -Wl,-Map=$@.map
	$(STRIP) $@

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $(LIBS) -o $@ $<
	$(CXX) -MM $(CFLAGS) $(LIBS) $< > $@.d

# pull in dependency info for *existing* .o files
-include $(ALL_DEPS)

.PHONY: clean cleanlibs
clean:
	@echo "Cleaning up directory."
	$(RM) $(LOOP_OBJS) $(RECG_OBJS) $(REG_OBJS) $(LOOP_PROG) $(RECG_PROG) $(REG_PROG) $(ALL_DEPS)

cleanlibs:
	@echo "Cleaning up libraries."
	make -C ./libsimplelog clean

install:
	install $(LOOP_PROG) $(PREFIX)
	install $(RECG_PROG) $(PREFIX)
	install $(REG_PROG) $(PREFIX)
	install ./libIDPParseParameter/*.so* $(LIB_INSTALL_DIR)
	install ./libuvc/*.so* $(LIB_INSTALL_DIR)
	install ./libsimplelog/*.so* $(LIB_INSTALL_DIR)
	install ./libFaceNeuralNetwork/*.so* $(LIB_INSTALL_DIR)

