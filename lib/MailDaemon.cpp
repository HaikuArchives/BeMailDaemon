#include <Messenger.h>
#include <Message.h>

#include <MailDaemon.h>

_EXPORT status_t Mail::CheckMail(bool send_queued_mail) {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage('mnow');
	
	if (send_queued_mail)
		daemon.SendMessage('msnd');
		
	return B_OK;
}

_EXPORT status_t Mail::SendQueuedMail() {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage('msnd');
		
	return B_OK;
}

_EXPORT int32 Mail::CountNewMessages(bool wait_for_fetch_completion) {
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

_EXPORT status_t Mail::QuitDaemon() {
	BMessenger daemon("application/x-vnd.Be-POST");
	if (!daemon.IsValid())
		return B_MAIL_NO_DAEMON;
	
	daemon.SendMessage(B_QUIT_REQUESTED);
		
	return B_OK;
}
