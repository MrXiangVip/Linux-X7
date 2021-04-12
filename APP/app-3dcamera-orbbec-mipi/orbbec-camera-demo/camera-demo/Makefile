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
VERSION = 0.1
PREFIX = /usr/local/bin

WARNINGS = -Wall
HOST_ROOTFS ?= $(OECORE_TARGET_SYSROOT)
ifeq ($(DEBUG), 1)
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O2 -g $(WARNINGS) -I$(HOST_ROOTFS) -I$(HOST_ROOTFS/usr/include) -I./Common -I./librs/inc -I./libodepth -I./libsimplelog/inc -I./
else
	CFLAGS = -DLINUX -DVERSION=\"$(VERSION)\" -O0 -g $(WARNINGS) -I$(HOST_ROOTFS) -I./Common -I./librs/inc -I./libodepth -I./libsimplelog/inc -I./
endif

COMM_SRCS += uart_handle.cpp
COMM_SRCS += hl_MsgQueue.cpp
COMM_SRCS += sys_upgrade.cpp
COMM_SRCS += per_calc.cpp
COMM_SRCS += sig_timer.cpp
COMM_SRCS += util_crc32.cpp
COMM_SRCS += function.cpp
COMM_SRCS += RSApiHandle.cpp
COMM_SRCS += orbbec_mipi_ctrl.cpp
RECG_SRCS = face_recgnize.cpp
RECG_SRCS += $(COMM_SRCS)
REG_SRCS = face_reg.cpp
REG_SRCS += $(COMM_SRCS)
FACE_SRCS = face_loop.cpp
FACE_SRCS += $(COMM_SRCS)

# Sets the output filename and object files
RECG_OBJS= $(RECG_SRCS:.cpp=.o)
RECG_DEPS= $(RECG_OBJS:.o=.d)

REG_OBJS= $(REG_SRCS:.cpp=.o)
REG_DEPS= $(REG_OBJS:.o=.d)

FACE_OBJS= $(FACE_SRCS:.cpp=.o)
FACE_DEPS= $(FACE_OBJS:.o=.d)

LIB_SIMPLELOG = -L./libsimplelog -lsimplelog -I./libsimplelog/inc
LIB_ODEPTH = -L./libodepth -lodepth
LIB_YUV = -L./libyuv -lyuv
LIB_RS = -L./librs -lReadFace
LIBS = -lpthread -lm -L$(HOST_ROOTFS)/usr/lib $(LIB_SIMPLELOG) $(LIB_RS) $(LIB_YUV) $(LIB_ODEPTH)

# pull in dependency info for *existing* .o files
-include $(DEPS)

all: $(RECG_PROG) $(REG_PROG) $(FACE_PROG)

$(RECG_PROG): $(RECG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(RECG_OBJS) $(LIBS) $(LDFLAGS)
	#$(STRIP) $@

$(REG_PROG): $(REG_OBJS)
	$(CXX) $(CFLAGS) -o $@ $(REG_OBJS) $(LIBS) $(LDFLAGS)
	#$(STRIP) $@

$(FACE_PROG): $(FACE_OBJS) 
	$(CXX) $(CFLAGS) -o $@ $(FACE_OBJS) $(LIBS) $(LDFLAGS)
	#$(STRIP) $@
	
%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<
	$(CXX) -MM $(CFLAGS) $< > $@.d

libs:
	make -C ./libsimplelog
	
clean:
	@echo "Cleaning up directory."
	$(RM) $(RECG_OBJS) $(REG_OBJS) $(FACE_OBJS) $(RECG_PROG) $(REG_PROG) $(FACE_PROG) $(RECG_DEPS) $(REG_DEPS) $(FACE_DEPS) 

install:
	install $(RECG_PROG) $(PREFIX)
	install $(REG_PROG) $(PREFIX)
	install $(FACE_PROG) $(PREFIX)

