#!/bin/sh
#invoke as follows:
#build_package.sh path_to_put_package_in version

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

mimeset -f $BUILD_PATH

