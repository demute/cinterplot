SUBDIRS := $(filter-out common/,$(wildcard */))
SUBDIRS := $(patsubst %/,%, $(SUBDIRS))

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean || true; done

