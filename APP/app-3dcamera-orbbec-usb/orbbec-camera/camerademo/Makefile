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
RECG_PROG = orbbec_rs_recg_face
REG_PROG = orbbec_rs_reg_face
VERSION = 0.1
PREFIX = /usr/local/bin

WARNINGS = -Wall
HOST_ROOTFS ?= $(OECORE_TARGET_SYSROOT)
ifeq ($(DEBUG), 1)
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 -g $(WARNINGS) -I$(HOST_ROOTFS) -I$(HOST_ROOTFS/usr/include) -I./Common -I./libopenni/inc -I./librs/inc -I./
else
	#CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O0 -g $(WARNINGS) -I$(HOST_ROOTFS) -I./Common -I./libopenni/inc -I./librs/inc -I./
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 $(WARNINGS) -I$(HOST_ROOTFS) -I./Common -I./libopenni/inc -I./librs/inc -I./
endif

COMM_SRCS += OpenNIEx.cpp
COMM_SRCS += RSApiHandle.cpp
COMM_SRCS += Common/OniSampleUtilities.cpp
FACE_SRCS = face_loop.cpp
FACE_SRCS += $(COMM_SRCS)
RECG_SRCS = face_recgnize.cpp
RECG_SRCS += $(COMM_SRCS)
REG_SRCS = face_reg.cpp
REG_SRCS += $(COMM_SRCS)

# Sets the output filename and object files
RECG_OBJS= $(RECG_SRCS:.cpp=.o)
RECG_DEPS= $(RECG_OBJS:.o=.o.d)

REG_OBJS= $(REG_SRCS:.cpp=.o)
REG_DEPS= $(REG_OBJS:.o=.o.d)

FACE_OBJS= $(FACE_SRCS:.cpp=.o)
FACE_DEPS= $(FACE_OBJS:.o=.o.d)

LIB_OPENNI = -I./libopenni/inc -L./libopenni/arm/Arm-Release-without-filter -lOpenNI2 #-lwayland-client -lwayland-cursor -lwayland-egl -ldl -lxkbcommon -lGL -lglfw3
LIB_YUV = -L./libyuv -lyuv
LIB_RS = -L./librs -lReadFace
LIBS = -lpthread -lm -L$(HOST_ROOTFS)/usr/lib $(LIB_OPENNI) $(LIB_RS) $(LIB_YUV)

# pull in dependency info for *existing* .o files
-include $(DEPS)

all: $(RECG_PROG) $(REG_PROG) $(FACE_PROG)

$(RECG_PROG): $(RECG_OBJS) Makefile
	$(CXX) $(CFLAGS) -o $@ $(RECG_OBJS) $(LIBS) $(LDFLAGS)
	$(STRIP) $@

$(REG_PROG): $(REG_OBJS) Makefile
	$(CXX) $(CFLAGS) -o $@ $(REG_OBJS) $(LIBS) $(LDFLAGS)
	$(STRIP) $@

$(FACE_PROG): $(FACE_OBJS) Makefile
	$(CXX) $(CFLAGS) -o $@ $(FACE_OBJS) $(LIBS) $(LDFLAGS)
	$(STRIP) $@
	
%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<
	$(CXX) -MM $(CFLAGS) $< > $@.d

clean:
	@echo "Cleaning up directory."
	$(RM) $(RECG_OBJS) $(REG_OBJS) $(FACE_OBJS) $(RECG_PROG) $(REG_PROG) $(FACE_PROG) $(RECG_DEPS) $(REG_DEPS) $(FACE_DEPS) 

install:
	install $(RECG_PROG) $(PREFIX)
	install $(REG_PROG) $(PREFIX)
	install $(FACE_PROG) $(PREFIX)

