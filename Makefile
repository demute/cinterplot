TOPDIR = .
include $(TOPDIR)/Makefile.common

.PHONY: all


PKGS = sdl2 libpng
PKGS_CFLAGS = $(foreach pkg,$(PKGS),--cflags $(pkg))
PKGS_LIBS   = $(foreach pkg,$(PKGS),--libs $(pkg))
CFLAGS     += $(shell $(PKGCONFIG) $(PKGS_CFLAGS))
LDFLAGS    += $(shell $(PKGCONFIG) $(PKGS_LIBS))

OBJS += cinterplot.o
OBJS += stream_buffer.o
OBJS += oklab.o
OBJS += savepng.o

EXAMPLES = $(wildcard examples/*/.)
.PHONY: run $(EXAMPLES)

TARGET=$(LIBDIR)/libcinterplot.$(LIBEXT)

all:$(TARGET)

ex:$(EXAMPLES)

examples:$(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C $@

$(LIBDIR)/libcinterplot.dylib:$(OBJS)
	mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) -o$@ $^ -shared -undefined suppress -flat_namespace

$(LIBDIR)/libcinterplot.so:$(OBJS)
	mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) -o$@ $^ -shared

%.so:$(OBJS)
	cat exports_mac.txt | ./gen_exports_linux.pl > exports_linux.txt
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.elf *.bin *.hex *.size *.dylib lib/*.so lib/*.dylib

cleanex:
	for dir in $(EXAMPLES); do \
		$(MAKE) -C $$dir clean; \
	done

cleanall:clean exclean
