#!/bin/sh

cd "$(dirname "$0")"

if pwd | grep " ";
  then alert "ディレクトリ名に空白を含めないでください。
インストールが正常にできません。
/boot/home/mydirectoryのように
空白を含まないディレクトリに
内容を移動してください。";
  exit 1;
fi

RETURN=`alert "Mail Daemon Replacementインストール

BeOSのメールデーモンは
システムに一つしか存在できません。
（理由はREADMEをお読みください）

「Purge」：
　保存しません
「Backup」：
　MDRを初めてインストールする時に、
　古いメールデーモン
　(mail_daemon/libmail.so/BeMail/E-mail)を
　保存します。
　（2回目以降は選択しないでください）
「Don't do anything!」：
　インストールを中止します。" "Purge" "Backup" "Don't do anything!"`

if [[ $RETURN = Purge ]]
then
	# note: we don't remove libmail.so, because it doesn't matter here, and there may be symlinks and things
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf" || BEOS:APP_SIG == "application/x-vnd.Be-MAIL" || BEOS:APP_SIG == "application/x-vnd.agmsmith.AGMSBayesianSpamServer"' | grep -v "`/bin/pwd`" | xargs rm -f
elif [[ $RETURN = Backup ]]
then
	query -a 'BEOS:APP_SIG == "application/x-vnd.Be-POST" || BEOS:APP_SIG == "application/x-vnd.Be-mprf" || BEOS:APP_SIG == "application/x-vnd.Be-MAIL" || BEOS:APP_SIG == "application/x-vnd.agmsmith.AGMSBayesianSpamServer" || name == libmail.so' | grep -v "`/bin/pwd`" | xargs zip -ym /boot/home/maildaemon.zip
else
	alert "Mail Daemon Replacementインストール
	
　バックアップを作成しませんでした。
　現在のバージョンが唯一のメールデーモンとなります。"
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
# English mode.
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/Inbox" "${HOME}/config/add-ons/mail_daemon/system_filters/受信箱"
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/Outbox" "${HOME}/config/add-ons/mail_daemon/system_filters/送信箱"
ln -f -s -v "${HOME}/config/add-ons/mail_daemon/system_filters/New Mail Notification" "${HOME}/config/add-ons/mail_daemon/system_filters/郵便通告方法"

sleep 1
/system/Deskbar &
sleep 1
/system/servers/mail_daemon &

sync
alert "Mail Daemon Replacementインストール

　インストールが完了しました。
　再インストールする際は再度
　パッケージの解凍から行ってください。"

# Prevent a second attempt to install since all our source files have been moved / deleted.
echo "This one use install script will now self destruct..."
rm install.japanese.sh
