TOPDIR = ../..
include $(TOPDIR)/Makefile.common

.PHONY: all

CFLAGS  += $(shell pkg-config --cflags gsl)

LDFLAGS += -L$(LIBDIR) -lcinterplot
LDFLAGS += $(shell pkg-config --libs rtmidi)
LDFLAGS += $(shell pkg-config --libs gsl)
LDFLAGS += $(shell pkg-config --libs sdl2)

CXXFLAGS += -std=c++11
CXXFLAGS += $(shell pkg-config --cflags rtmidi)

OBJS  = midilib.o
OBJS += randlib.o
OBJS += common.o
OBJS += audio.o
OBJS += kiss_fft.o

.PHONY: run

TARGET=app
all:$(TARGET)

run: app
	@echo "[running ./app]"
	@./app && echo "[process completed successfully]" || echo "[process completed abnormally]"

app: $(OBJS) app.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib app
