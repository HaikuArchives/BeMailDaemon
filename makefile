# makefile for all sample code

SUBDIRS = \
	lib \
	daemon \
	config \
	inbound_protocols \
	outbound_protocols \
	inbound_filters \
	outbound_filters \
	system_filters

#all:
#	-@for f in $(SUBDIRS) ; do \
#		$(MAKE) -C $$f -f makefile; \
#	done
#

default .DEFAULT :
	cp makefile-engine.MailD $(BUILDHOME)/etc/makefile-engine.MailD
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile $@; \
	done

#install: all
#	mkdir bin > /dev/null 2>&1 || true
#	mv protocols/{pop3/*/POP3,smtp/*/SMTP} filters/*/*/compatibility daemon/*/mail_daemon config/*/E-mail lib/libmail2.so bin
#	./install.sh
