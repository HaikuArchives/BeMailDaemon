#!/bin/sh
#invoke as follows:
#build_package.sh path_to_put_package_in version

if [ "$1" = "" ]; then
	echo "Usage: build_package.sh path_to_put_package_in version"
	echo "   All paths must be absolute!"
	exit
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

cd "$(dirname "$0")"/..
mkdir -p $BUILD_PATH/bin

copyattr -d build_utils/install.sh $BUILD_PATH
chmod a+x $BUILD_PATH/install.sh
copyattr -d bemail/obj.$CPU/BeMail $BUILD_PATH/bin
copyattr -d AGMSBayesianSpamServer/obj.$CPU/AGMSBayesianSpamServer $BUILD_PATH/bin
copyattr -d AGMSBayesianSpamServer/SampleDatabase $BUILD_PATH/bin
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

cp -d documentation/read_us/* $BUILD_PATH

mkdir -p $BUILD_PATH/AGMSBayesianSpamDocumentation/pictures
cp documentation/AGMSBayesianSpam/*html $BUILD_PATH/AGMSBayesianSpamDocumentation/
cp documentation/AGMSBayesianSpam/pictures/*png $BUILD_PATH/AGMSBayesianSpamDocumentation/pictures


mimeset -f $BUILD_PATH

