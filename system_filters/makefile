# makefile for all sample code

SUBDIRS = \
	Folder \
	Parser \
	OutFolder \
	Notifier

all: default


default .DEFAULT :
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -j $(sysinfo -cpu | head -c 2) -C $$f -f makefile $@ || exit -1; \
	done

install:
	mkdir -p /boot/home/config/add-ons/mail_daemon/system_filters
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -j $(sysinfo -cpu | head -c 2) -C $$f -f makefile install $@ || exit -1; \
	done
