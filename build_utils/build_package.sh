#!/bin/sh
# Invoke as follows:
# build_package.sh path_to_put_package_in version
#
# Note that the choice of file to copy for the install script that the user
# sees, is specified by the main make file.  It writes the file name to the
# LanguageSpecificInstallFileName file.  The Japanese version of the installer
# will ask its questions in Japanese, and rename files (some of the filters)
# which need Japanese names.

if [ "$1" = "" ]; then
	echo "Usage: build_package.sh path_to_put_package_in version_string"
	echo "   All paths must be absolute!"
	exit
fi

cd "$(dirname "$0")"/..

if test ! -e "build_utils/LanguageSpecificInstallFileName"; then
	echo "install.sh" >build_utils/LanguageSpecificInstallFileName ;
fi

if test -e "build_utils/`cat build_utils/LanguageSpecificInstallFileName`";	then
	echo "Will use the `cat build_utils/LanguageSpecificInstallFileName` for the install script.";
else
	echo "Sorry, can't find the install script named `cat build_utils/LanguageSpecificInstallFileName`.";
	exit -1;
fi

#all paths must be absolute!

if [ $(uname -m) = "BePC" ]; then
	CPU=x86
else
	CPU=ppc
fi

BUILD_PATH=$1/mail_daemon_$2_$CPU

# if [ #(echo $BUILD_PATH | grep -v '//*') -neq 0 ]; then 
#      BUILD_PATH=$(pwd)/$1/mail_daemon_$2_$CPU; 
# fi 

echo $BUILD_PATH

mkdir -p $BUILD_PATH/bin

copyattr -d "build_utils/`cat build_utils/LanguageSpecificInstallFileName`" $BUILD_PATH
chmod a+x "$BUILD_PATH/`cat build_utils/LanguageSpecificInstallFileName`"
copyattr -d bemail/obj.$CPU/BeMail $BUILD_PATH/bin
copyattr -d AGMSBayesianSpamServer/obj.$CPU/AGMSBayesianSpamServer $BUILD_PATH/bin
# Note name change from SampleDatabase to SampleSpamDatabase, to make it more obvious what it is to the user.
copyattr -d AGMSBayesianSpamServer/SampleDatabase $BUILD_PATH/bin/SampleSpamDatabase
copyattr -d AGMSBayesianSpamServer/SoundSpam $BUILD_PATH/bin/SoundSpam
copyattr -d AGMSBayesianSpamServer/SoundGenuine $BUILD_PATH/bin/SoundGenuine
copyattr -d AGMSBayesianSpamServer/SoundUncertain $BUILD_PATH/bin/SoundUncertain
copyattr -d daemon/obj.$CPU/mail_daemon $BUILD_PATH/bin
copyattr -d lib/libmail.so $BUILD_PATH/bin
copyattr -d config/obj.$CPU/E-mail $BUILD_PATH/bin

for f in inbound_* outbound_* system_* ; do
	 if [ "$f" != "CVS" -a -d "$f" ]; then
		mkdir -p $BUILD_PATH/bin/addons/$f
		cd $f
		for g in * ; do
			 if [ "$g" != "CVS" -a -d "$g" ]; then
				cd $g/obj.$CPU
				OBJ_NAME=$(ls | egrep -v '\..*$')
				echo copyattr -d $pwd/$OBJ_NAME $BUILD_PATH/bin/addons/$f/$OBJ_NAME
				copyattr -d "$OBJ_NAME" "$BUILD_PATH/bin/addons/$f/$OBJ_NAME" 
				cd ../..
			fi
		done
		cd ..
	fi
done

cp documentation/read_us/README $BUILD_PATH
mdir $BUILD_PATH/Extra\ Documentation
cp documentation/read_us/* $BUILD_PATH/Extra\ Documentation
rm -f $BUILD_PATH/Extra\ Documentation/README
cp build_utils/ExtraMenuLinksForR5Tracker.zip $BUILD_PATH/bin
mkdir -p $BUILD_PATH/Extra\ Documentation/AGMSBayesianSpamDocumentation/pictures
cp -v documentation/AGMSBayesianSpam/*html $BUILD_PATH/Extra\ Documentation/AGMSBayesianSpamDocumentation/
cp documentation/AGMSBayesianSpam/pictures/*png $BUILD_PATH/Extra\ Documentation/AGMSBayesianSpamDocumentation/pictures/
mkdir -p $BUILD_PATH/Extra\ Documentation/Replacement\ Email\ Preferences\ Panel\ Users\ Guide/art
cp -v documentation/Replacement\ Email\ Preferences\ Panel\ Users\ Guide/*html $BUILD_PATH/Extra\ Documentation/Replacement\ Email\ Preferences\ Panel\ Users\ Guide/
cp documentation/Replacement\ Email\ Preferences\ Panel\ Users\ Guide/art/*.* $BUILD_PATH/Extra\ Documentation/Replacement\ Email\ Preferences\ Panel\ Users\ Guide/art/
mkdir -p $BUILD_PATH/Extra\ Documentation/Mass\ Mailing
cp --preserve --verbose documentation/Mass\ Mailing/* $BUILD_PATH/Extra\ Documentation/Mass\ Mailing/

mimeset -f $BUILD_PATH
