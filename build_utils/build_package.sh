#invoke as follows:
#build_package.sh path_to_put_package_in version

cd "$(dirname "$0")"/..

if [ $(uname -m) = "BePC" ]; then
	CPU=x86
else
	CPU=ppc
fi

BUILD_PATH=$1/mail_daemon_$2_$CPU
mkdir $BUILD_PATH
mkdir $BUILD_PATH/bin

cp build_utils/install.sh $BUILD_PATH
chmod a+x $BUILD_PATH/install.sh
cp bemail/obj.$CPU/BeMail $BUILD_PATH/bin
cp daemon/obj.$CPU/mail_daemon $BUILD_PATH/bin
cp lib/libmail.so $BUILD_PATH/bin
cp config/obj.$CPU/E-mail $BUILD_PATH/bin

for f in inbound_* outbound_* system_* ; do
	 if [ "$f" != "CVS" -a -d "$f" ]; then
		mkdir -p $BUILD_PATH/bin/addons/$f
		cd $f
		for g in * ; do
			 if [ "$g" != "CVS" -a -d "$g" ]; then
				cd $g/obj.$CPU
				OBJ_NAME=$(ls | egrep -v '\..*$')
				cp $OBJ_NAME $BUILD_PATH/bin/addons/$f/$OBJ_NAME
				cd ../..
			fi
		done
		cd ..
	fi
done

cp documentation/read_us/* $BUILD_PATH

mimeset -f $BUILD_PATH

