#!/bin/sh

cd "$(dirname "$0")"

if pwd | grep " ";
  then alert "Please install from a directory with no spaces in the path name, the current one (`pwd`) has spaces.  Just move all the files to somewhere that doesn't have spaces, like /boot/home/mydirectory.  If there are spaces, the install script malfunctions.  Sorry.";
  exit 1;
fi

RETURN=`alert "There can only be ONE version of mail_daemon on the system at one time (see the enclosed README file for why).

Choose 'Backup' if you wish to keep your old mail_daemon, E-mail preferences app, and BeMail, and this is the first time you're installing the Mail Daemon Replacement.  Otherwise, you should choose 'Purge' to clear the other versions from your system." "Purge" "Backup" "Don't do anything!"`

if [[ $RETURN = Purge ]]
then
	# note: we don't remove libmail.so, because it doesn't matter here, and there may be symlinks and things
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf" || BEOS:APP_SIG == "application/x-vnd.Be-MAIL" || BEOS:APP_SIG == "application/x-vnd.agmsmith.AGMSBayesianSpamServer"' | grep -v "`/bin/pwd`" | xargs rm -f
elif [[ $RETURN = Backup ]]
then
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf" || BEOS:APP_SIG == "application/x-vnd.Be-MAIL" || BEOS:APP_SIG == "application/x-vnd.agmsmith.AGMSBayesianSpamServer" || name == libmail.so' | grep -v "`/bin/pwd`" | xargs zip -ym /boot/home/maildaemon.zip
else
	alert "No backup will be done.  That means it's up to YOU to purge all of your old mail_daemons and ensure that the new version is the only version."
	exit -1
fi

quit "application/x-vnd.Be-POST"
quit "application/x-vnd.Be-TSKB"
quit "application/x-vnd.agmsmith.AGMSBayesianSpamServer"

rm -rf ~/config/add-ons/mail_daemon/*
rm -f /system/servers/mail_daemon /boot/beos/preferences/E-mail /boot/beos/apps/BeMail
rm -f ~/config/lib/libmail2.so ~/config/lib/libnumail2.so /system/lib/libmail.so

mkdir -p ~/config/add-ons/mail_daemon

copyattr -d -m bin/libmail.so /boot/beos/system/lib/libmail.so
copyattr -d -m -r bin/addons/* ~/config/add-ons/mail_daemon
copyattr -d -m bin/mail_daemon /system/servers/mail_daemon
copyattr -d -m bin/E-mail /boot/beos/preferences/E-mail
copyattr -d -m bin/BeMail /boot/beos/apps/BeMail

mkdir -p ~/config/settings/Mail/Menu\ Links
unzip -n bin/ExtraMenuLinksForR5Tracker.zip -d ~/config/settings/Mail/Menu\ Links
rm bin/ExtraMenuLinksForR5Tracker.zip

# Set up the spam classifier server.
copyattr -d -m bin/AGMSBayesianSpamServer ~/config/bin/AGMSBayesianSpamServer
mkdir -p ~/config/settings/AGMSBayesianSpam
if test ! -e "${HOME}/config/settings/AGMSBayesianSpam/AGMSBayesianSpam Database"; then
    cp bin/SampleSpamDatabase "${HOME}/config/settings/AGMSBayesianSpam/AGMSBayesianSpam Database";
fi
copyattr -d -m bin/SoundGenuine "${HOME}/config/settings/AGMSBayesianSpam/SoundGenuine"
copyattr -d -m bin/SoundSpam "${HOME}/config/settings/AGMSBayesianSpam/SoundSpam"
copyattr -d -m bin/SoundUncertain "${HOME}/config/settings/AGMSBayesianSpam/SoundUncertain"
# Create the MIME types and indices needed by the spam server.
~/config/bin/AGMSBayesianSpamServer InstallThings

# Patch up things that need Japanese names.  Do it here before the daemon is
# started.  A link (rather than renaming) is done so that saved chain settings,
# which use the file names of the add-ons, will still work if they were made in
# English mode.  Also done in the English version, in case someone was using
# Japanese and switched back to English.
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/Inbox" "${HOME}/config/add-ons/mail_daemon/system_filters/受信箱"
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/Outbox" "${HOME}/config/add-ons/mail_daemon/system_filters/送信箱"
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/New Mail Notification" "${HOME}/config/add-ons/mail_daemon/system_filters/郵便通告方法"

sleep 1
/system/Deskbar &
sleep 1
/system/servers/mail_daemon &

sync
alert "Installation Complete."

# Prevent a second attempt to install since all our source files have been moved / deleted.
echo "This one use install script will now self destruct..."
rm install.sh
