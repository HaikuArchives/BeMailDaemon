# makefile for all sample code

SUBDIRS = \
	lib \
	daemon \
	config \
	inbound_protocols \
	outbound_protocols \
	inbound_filters \
	outbound_filters \
	system_filters \
	bemail \
	AGMSBayesianSpamServer

default .DEFAULT :
	-cp makefile-engine.MailD $(BUILDHOME)/etc/makefile-engine.MailD
	
	# must install lib to make the rest.
	$(MAKE) -C lib install
			
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile $@ || exit -1; \
	done

clean:
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile clean || exit -1; \
	done


#install: all
#	mkdir bin > /dev/null 2>&1 || true
#	mv protocols/{pop3/*/POP3,smtp/*/SMTP} filters/*/*/compatibility daemon/*/mail_daemon config/*/E-mail lib/libmail2.so bin
#	./install.sh
