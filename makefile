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
	@echo "Build_package now set to use `cat build_utils/LanguageSpecificInstallFileName` for the language dependant installer script."


clean:
	-@for f in $(SUBDIRS) ; do \
		$(MAKE) -C $$f -f makefile clean || exit -1; \
	done
