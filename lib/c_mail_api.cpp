#include <E-mail.h>
#include <MailDaemon.h>
#include <Directory.h>

_EXPORT status_t	check_for_mail(int32 * incoming_count) {
	status_t err = MailDaemon::CheckMail(true);
	if (err < B_OK)
		return err;
		
	if (incoming_count != NULL)
		*incoming_count = MailDaemon::CountNewMessages(true);
		
	return B_OK;
}
	
_EXPORT status_t	send_queued_mail(void) {
	return MailDaemon::SendQueuedMail();
}

_EXPORT int32		count_pop_accounts(void) {
	BDirectory dir("/boot/home/config/settings/Mail/chains/inbound");
	return dir.CountEntries();
}