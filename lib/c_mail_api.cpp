#include <E-mail.h>
#include <MailDaemon.h>

_EXPORT status_t	check_for_mail(int32 * /*incoming_count*/) {
	//----How do we do incoming count?
	
	return MailDaemon::CheckMail(true);
}
	
_EXPORT status_t	send_queued_mail(void) {
	return MailDaemon::SendQueuedMail();
}