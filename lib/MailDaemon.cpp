#include <Messenger.h>
#include <Message.h>

class _EXPORT MailDaemon;

#include <MailDaemon.h>

status_t MailDaemon::CheckMail(bool send_queued_mail) {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage('mnow');
	
	if (send_queued_mail)
		daemon.SendMessage('msnd');
		
	return B_OK;
}

status_t MailDaemon::SendQueuedMail() {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage('msnd');
		
	return B_OK;
}

status_t MailDaemon::Quit() {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage(B_QUIT_REQUESTED);
		
	return B_OK;
}