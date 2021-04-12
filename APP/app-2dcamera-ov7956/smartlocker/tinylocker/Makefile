STRIP ?=$(CROSS_COMPILE)strip
CFLAGS += -std=c++11 -O3

INCLUDE  = -I./ -I./include/

LIB_SIMPLELOG = -L./libsimplelog -lsimplelog -I./libsimplelog/inc
LIB = -L./libs $(LIB_SIMPLELOG) -lReadFace -lpthread -lstdc++

OBJS = RSApiHandle.o tinylocker.o facereg.o faceloop.o function.o util_crc32.o sig_timer.o

all: $(OBJS)
	$(CXX) RSApiHandle.o faceloop.o function.o util_crc32.o sig_timer.o libs/libyuv.a $(CFLAGS) $(INCLUDE) $(LIB) -o faceloop
	$(STRIP) faceloop
	$(CXX) RSApiHandle.o tinylocker.o function.o util_crc32.o sig_timer.o libs/libyuv.a $(CFLAGS) $(INCLUDE) $(LIB) -o tinylocker
	$(STRIP) tinylocker
	$(CXX) RSApiHandle.o facereg.o function.o util_crc32.o sig_timer.o libs/libyuv.a $(CFLAGS) $(INCLUDE) $(LIB) -o facereg
	$(STRIP) facereg

$(OBJS): %.o : %.cpp
	$(CXX) $< -c  $(CFLAGS) $(INCLUDE) $(LIB) -o $@
   

libs:
	make -C ./libsimplelog
cleanlibs:
	@echo "Cleaning up libraries."
	make -C ./libsimplelog clean
	
.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f tinylocker facereg faceloop

