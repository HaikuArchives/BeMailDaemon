# makefile for all sample code

VERSION=2.3.2

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

ifeq ($(shell uname -m), BePC)
	CPU = x86
else
	CPU = ppc
endif

MDR_PATH = $(shell pwd)

ifeq ($(shell ls 2>/dev/null -1 /boot/develop/headers/be/bone/bone_api.h), /boot/develop/headers/be/bone/bone_api.h)
	CPU = bone
endif

default .DEFAULT :
	-cp makefile-engine.MailD $(BUILDHOME)/etc/makefile-engine.MailD

	# must install lib to make the rest.
	$(MAKE) -j $(shell sysinfo -cpu | head -c 2) -C lib install

	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -j $(shell sysinfo -cpu | head -c 2) -C $$f -f makefile $@ || exit -1; \
	done

#	SETTING: set the language for the installer.  Use
#	"make LANGUAGE=ENGLISHUSA" or "make LANGUAGE=JAPANESE".  Default is
#	ENGLISHUSA.  The name of the file will be stored in
#	build_utils/LanguageSpecificInstallFileName and later used by
#	build_utils/build_package.sh when it copies the appropriate file.
ifeq ($(LANGUAGE)x, JAPANESEx)
	@echo "install.japanese.sh" >build_utils/LanguageSpecificInstallFileName
else
	@echo "install.sh" >build_utils/LanguageSpecificInstallFileName
endif
	@echo "Build_package now set to use `cat build_utils/LanguageSpecificInstallFileName` for the language dependent installer script."

package :: default
	./build_utils/build_package.sh $(shell pwd) v$(VERSION)
	zip -rym mail_daemon_v$(VERSION)_$(CPU).zip mail_daemon_v$(VERSION)_$(CPU)

srcpackage:
	mimeset *
	rm -f /tmp/mail_daemon_v$(VERSION)_src
	ln -s $(shell pwd) /tmp/mail_daemon_v$(VERSION)_src
	sh -c "cd /tmp; zip -r $(MDR_PATH)/mail_daemon_v$(VERSION)_src.zip mail_daemon_v$(VERSION)_src -x mail_daemon_v$(VERSION)_src/CVS/ mail_daemon_v$(VERSION)_src/CVSROOT/ mail_daemon_v$(VERSION)_src/CVS/* mail_daemon_v$(VERSION)_src/CVSROOT/* mail_daemon_v$(VERSION)_src/*/obj.*/*  mail_daemon_v$(VERSION)_src/*/obj.*/  mail_daemon_v$(VERSION)_src/*/*/obj.*/*  mail_daemon_v$(VERSION)_src/*/*/obj.*/ mail_daemon_v$(VERSION)_src/*/CVS/* mail_daemon_v$(VERSION)_src/*/CVS/ mail_daemon_v$(VERSION)_src/*/*/CVS/* mail_daemon_v$(VERSION)_src/*/*/CVS/ */.cvsignore mail_daemon_v$(VERSION)_src/*/*/.cvsignore mail_daemon_v$(VERSION)_src/lib/libmail.so mail_daemon_v$(VERSION)_src/build_utils/LanguageSpecificInstallFileName"
	rm /tmp/mail_daemon_v$(VERSION)_src

clean:
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile clean || exit -1; \
	done
