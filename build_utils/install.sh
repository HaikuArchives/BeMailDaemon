#!/bin/sh

cd "$(dirname "$0")"

RETURN=`alert "There can only be ONE version of mail_daemon on the system at one time (see the enclosed README file for why).  Choose 'Backup' if you wish to keep your old mail_daemon and E-mail preferences app, and this is the first time you're installing the Mail Daemon Replacement.  Otherwise, you should choose 'Purge' to clear the other versions from your system." "Purge" "Backup" "Don't do anything!"`

if [[ $RETURN = "Purge" ]]
then
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf"' \
	| grep -v "`/bin/pwd`" \
	| xargs rm
elif [[ $RETURN = "Backup" ]]
then 
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf" || name == libmail.so' \
	| grep -v "`/bin/pwd`" \
	| xargs zip -ym /boot/home/maildaemon.zip
else 
	alert "No backup will be done.  That means it's up to YOU to purge all of your old mail_daemons and ensure that the new version is the only version."
fi

quit "application/x-vnd.Be-POST"
quit "application/x-vnd.Be-TSKB"

rm -rf ~/config/add-ons/mail_daemon/*

mkdir -p ~/config/add-ons/mail_daemon

rm -f ~/config/lib/libmail2.so ~/config/lib/libnumail2.so /boot/system/lib/libmail.so
copyattr -d -m bin/libmail.so /boot/system/lib/libmail.so

copyattr -d -m -r bin/addons/* ~/config/add-ons/mail_daemon

rm -f /boot/beos/system/servers/mail_daemon /boot/beos/preferences/E-mail
copyattr -d -m bin/mail_daemon /boot/beos/system/servers/mail_daemon
copyattr -d -m bin/E-mail /boot/beos/preferences/E-mail
sleep 1
/boot/beos/system/Deskbar &
sleep 1
/boot/beos/system/servers/mail_daemon &

alert "Installation Complete"
