SUBDIRS := $(wildcard package_*)

all:
	@for dir in $(SUBDIRS); do make -C $$dir; done

.PHONY: all

