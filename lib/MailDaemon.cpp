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

int32 MailDaemon::CountNewMessages(bool wait_for_fetch_completion) {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	BMessage reply;
	BMessage first('mnum');
	
	if (wait_for_fetch_completion)
		first.AddBool("wait_for_fetch_done",true);
		
	daemon.SendMessage(&first,&reply);	
		
	return reply.FindInt32("num_new_messages");
}

status_t MailDaemon::Quit() {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage(B_QUIT_REQUESTED);
		
	return B_OK;
}
