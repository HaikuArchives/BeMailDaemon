# makefile for all sample code

SUBDIRS = \
	CompatibilityFilter

all: default

default .DEFAULT :
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile $@; \
	done

install:
	mkdir -p /boot/home/config/add-ons/mail_daemon/inbound_filters
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile install $@; \
	done