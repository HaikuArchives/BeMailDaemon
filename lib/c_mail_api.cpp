#include <E-mail.h>
#include <Directory.h>
#include <List.h>

#include <string.h>

#include <MailDaemon.h>
#include <MailSettings.h>
#include <crypt.h>

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

_EXPORT status_t	get_pop_account(mail_pop_account* account, int32 index) {
	status_t err = B_OK;
	const char *password, *passwd;
	BMessage settings;
	
	BList chains;
	MailSettings::InboundChains(&chains);
	
	MailChain *chain = (MailChain *)(chains.ItemAt(index));
	if (chain == NULL) {
		err = B_BAD_INDEX;
		goto clean_up; //------Eek! A goto!
	}
	
	chain->GetFilter(0,&settings);
	strcpy(account->pop_name,settings.FindString("username"));
	strcpy(account->pop_host,settings.FindString("server"));
	strcpy(account->real_name,chain->MetaData()->FindString("real_name"));
	strcpy(account->reply_to,chain->MetaData()->FindString("reply_to"));
	
	password = settings.FindString("password");
	passwd = get_passwd(&settings,"cpasswd");
	if (passwd)
		password = passwd;
	strcpy(account->pop_password,password);
	
	//-------Note that we don't do the scheduling flags
	
  clean_up:
	for (int32 i = 0; i < chains.CountItems(); i++)
		delete chains.ItemAt(i);
		
	return err;
}

