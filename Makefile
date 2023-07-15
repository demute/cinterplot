.PHONY: all

CC      = gcc
CXX     = g++
 
CFLAGS  = -g -ggdb -Wall -Wno-deprecated -O3 -D_BSD_SOURCE
CFLAGS += -Wall -Wno-deprecated -Os -I../core -ferror-limit=5 -Wconversion -Werror -Wno-unused-function
CFLAGS += $(shell pkg-config --cflags sdl2)

LDFLAGS  = -lpthread
LDFLAGS += -Wl,-exported_symbols_list,exports.txt
LDFLAGS += $(shell pkg-config --libs sdl2)

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

EXAMPLES = $(wildcard examples/*/.)
.PHONY: run $(EXAMPLES)

TARGET=libcinterplot.dylib
all:$(TARGET) $(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C $@

libcinterplot.dylib:$(OBJS)
	$(CC) $(LDFLAGS) -o$@ $^ -shared -undefined suppress -flat_namespace

examples: $(EXAMPLES)

%.so:$(OBJS)
	g++ -shared -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib
	for dir in $(EXAMPLES); do \
		$(MAKE) -C $$dir clean; \
	done
