.PHONY: all

CC      = gcc
CXX     = g++
 
CFLAGS  = -g -ggdb -Wall -Wno-deprecated -O3 -D_BSD_SOURCE
CFLAGS += -Wall -Wno-deprecated -Os -I../core -ferror-limit=5 -Wconversion -Werror -Wno-unused-function
CFLAGS += $(shell pkg-config --cflags sdl2)
CFLAGS += $(shell pkg-config --cflags sdl2_image)

LDFLAGS += $(shell pkg-config --libs sdl2)
LDFLAGS += $(shell pkg-config --libs sdl2_image)

LDFLAGS += -lpthread
LDFLAGS += -Wl,-exported_symbols_list,exports.txt

OBJS += common.o
OBJS += cinterplot.o
OBJS += stream_buffer.o
OBJS += oklab.o

#UNAME=$(shell uname)
#ifeq ($(UNAME),Darwin)
#	LDFLAGS += -DHAVE_SDL -Wl,-framework,Cocoa
#else
#	LDFLAGS += -DHAVE_SDL
#endif

.PHONY: run

TARGET=libcinterplot.dylib
all:$(TARGET)

libcinterplot.dylib:$(OBJS)
	$(CC) $(LDFLAGS) -o$@ $^ -shared -undefined suppress -flat_namespace

%.so:$(OBJS)
	g++ -shared -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib
