# makefile for all sample code

SUBDIRS = Fortune

all: default

default .DEFAULT :
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -j $(shell sysinfo -cpu | head -c 2) -C $$f -f makefile $@ || exit -1; \
	done

install:
	mkdir -p /boot/home/config/add-ons/mail_daemon/outbound_filters
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -j $(shell sysinfo -cpu | head -c 2) -C $$f -f makefile install $@ || exit -1; \
	done
