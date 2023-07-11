.PHONY: all

CC      = gcc
CXX     = g++
 
CFLAGS  = -I../.. -g -ggdb -Wall -Wno-deprecated -O3 -D_BSD_SOURCE
CFLAGS += -Wall -Wno-deprecated -Os -ferror-limit=5 -Wconversion -Werror -Wno-unused-function
CFLAGS += $(shell pkg-config --cflags gsl)

LDFLAGS += $(shell pkg-config --libs rtmidi)
LDFLAGS += $(shell pkg-config --libs gsl)
LDFLAGS += -lcinterplot

CXXFLAGS  = -Wall -Wno-deprecated -Os -I../.. -std=c++11
CXXFLAGS += $(shell pkg-config --cflags rtmidi)

OBJS  = midilib.o
OBJS += randlib.o
OBJS += common.o

.PHONY: run

TARGET=app
all:$(TARGET)

run: app
	@echo "[running ./app]"
	@./app && echo "[process completed successfully]" || echo "[process completed abnormally]"

app: $(OBJS) app.o
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib app
