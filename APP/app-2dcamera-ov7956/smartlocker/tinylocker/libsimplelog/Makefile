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
AR ?= ar
CP ?= cp

LOG_LIBNAME = libsimplelog
LIBVERSION = 0.1
LOG_SOVERSION = 1
PREFIX = /usr/local/lib

SHARED = so
STATIC = a

LOG_SHARED = $(LOG_LIBNAME).$(SHARED)
LOG_SHARED_VERSION = $(LOG_LIBNAME).$(SHARED).$(LIBVERSION)
LOG_SHARED_SO = $(LOG_LIBNAME).$(SHARED).$(LOG_SOVERSION)
LOG_STATIC = $(LOG_LIBNAME).$(STATIC)

WARNINGS = -Wall
HOST_ROOTFS ?= $(OECORE_TARGET_SYSROOT)
ifeq ($(DEBUG), 1)
	CFLAGS = -DLINUX -DVERSION=\"$(LIBVERSION)\" -O0 -pipe -g -feliminate-unused-debug-types -fPIC $(WARNINGS) -I$(HOST_ROOTFS) -I./inc
else
	CFLAGS = -DLINUX -DVERSION=\"$(LIBVERSION)\" -O2 -pipe $(WARNINGS) -feliminate-unused-debug-types -fPIC -I$(HOST_ROOTFS) -I./inc
endif

SRCS = src/log.c

# Sets the output filename and object files
OBJS= $(SRCS:.c=.o)
DEPS= $(OBJS:.o=.d)

# pull in dependency info for *existing* .o files
-include $(DEPS)

all: shared static
	$(CP) inc/log.h inc/libsimplelog.h

shared: $(LOG_SHARED)

static: $(LOG_STATIC)

$(LOG_SHARED): $(OBJS)
	$(CC) $(CFLAGS) --shared -o $@ $(OBJS)
	#$(STRIP) $@

$(LOG_STATIC): $(OBJS)
	$(AR) rv $(LOG_STATIC) $?
	#$(STRIP) $@

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
	$(CC) -MM $(CFLAGS) $< > $@.d

clean:
	@echo "Cleaning up directory."
	$(RM) $(OBJS) $(LOG_SHARED) $(LOG_STATIC) $(DEPS) inc/libsimplelog.h

