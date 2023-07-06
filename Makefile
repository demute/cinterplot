.PHONY: all

CC      = gcc
CXX     = g++
LDFLEX  = -ll
LIBEXT  = dylib

CFLAGS  = -g -ggdb -Wall -Wno-deprecated -O3 -D_BSD_SOURCE -I$(TOPDIR)/core -I$(DRIVERDIR)
CFLAGS += -Wall -Wno-deprecated -Os -I../core -ferror-limit=5
CFLAGS += $(shell pkg-config --cflags gsl)
CFLAGS += $(shell pkg-config --cflags sdl2)
CFLAGS += $(shell pkg-config --cflags sdl2_image)

LDFLAGS += $(shell pkg-config --libs rtmidi)
LDFLAGS += $(shell pkg-config --libs gsl)
LDFLAGS += $(shell pkg-config --libs sdl2)
LDFLAGS += $(shell pkg-config --libs sdl2_image)

LDFLAGS += -lpthread

CXXFLAGS  = -Wall -Wno-deprecated -Os -I../core -std=c++11
CXXFLAGS += $(shell pkg-config --cflags rtmidi)
CXXFLAGS += $(shell pkg-config --cflags gsl)
CXXFLAGS += $(shell pkg-config --cflags sdl2)
CXXFLAGS += $(shell pkg-config --cflags sdl2_image)

OBJS  = midilib.o
OBJS += common.o
OBJS += plotlib.o
OBJS += stream_buffer.o
OBJS += randlib.o
OBJS += oklab.o

#UNAME=$(shell uname)
#ifeq ($(UNAME),Darwin)
#	LDFLAGS += -DHAVE_SDL -Wl,-framework,Cocoa
#else
#	LDFLAGS += -DHAVE_SDL
#endif

.PHONY: run

TARGET=main
all:$(TARGET)

run: main
	@echo "[running ./main]"
	@./main && echo "[process completed successfully]" || echo "[process completed abnormally]"

main: $(OBJS) main.o
	$(CXX) -o $@ $^ $(LDFLAGS)

%.dylib:$(OBJS)
	g++ -shared -o $@ $^ -framework CoreMIDI -framework CoreAudio -framework CoreFoundation $(LDFLAGS)

%.so:$(OBJS)
	g++ -shared -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib main
