TOPDIR = ../..
include $(TOPDIR)/Makefile.common

.PHONY: all

CFLAGS  += $(shell pkg-config --cflags gsl)

LDFLAGS += -L$(LIBDIR) -lcinterplot
LDFLAGS += $(shell pkg-config --libs gsl)
LDFLAGS += $(shell pkg-config --libs sdl2)

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
