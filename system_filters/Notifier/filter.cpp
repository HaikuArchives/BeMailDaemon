/* New Mail Notification - notifies incoming e-mail
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>
#include <String.h>
#include <Alert.h>
#include <Beep.h>
#include <Application.h>

#include <MailAddon.h>
#include <ChainRunner.h>
#include <status.h>

#include <MDRLanguage.h>

#include "ConfigView.h"

using namespace Zoidberg;


class NotifyCallback : public Mail::ChainCallback {
	public:
		NotifyCallback (int32 notification_method, Mail::Chain *us);
		virtual void Callback(status_t result);
		
		uint32 num_messages;
	private:
		Mail::Chain *chain;
		int32 strategy;
};

class NotifyFilter : public Mail::Filter
{
  public:
	NotifyFilter(BMessage*,Mail::ChainRunner*);
	virtual status_t InitCheck(BString *err);
	virtual status_t ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, const char* io_uid
	);
	
  private:
  	NotifyCallback *callback;
};


NotifyFilter::NotifyFilter(BMessage* msg,Mail::ChainRunner *runner)
	: Mail::Filter(msg)
{
	callback = new NotifyCallback(msg->FindInt32("notification_method"),runner->Chain());
	
	runner->RegisterProcessCallback(callback);
}

status_t NotifyFilter::InitCheck(BString* err)
{
	return B_OK;
}

status_t NotifyFilter::ProcessMailMessage(BPositionIO**, BEntry*, BMessage*, BPath*, const char*)
{
	callback->num_messages ++;
	
	return B_OK;
}

NotifyCallback::NotifyCallback (int32 notification_method, Mail::Chain *us) : 
	strategy(notification_method),
	chain(us),
	num_messages(0)
{
}

void NotifyCallback::Callback(status_t result) {
	if (num_messages == 0)
		return;
	
	if (strategy & do_beep)
		system_beep("New E-mail");
		
	if (strategy & alert) {
		BString text;
		MDR_DIALECT_CHOICE (
		text << "You have " << num_messages << " new message" << ((num_messages != 1) ? "s" : "")
		<< " for " << chain->Name() << ".",

		text << chain->Name() << "より\n" << num_messages << " 通のメッセージが届きました");

		Mail::ShowAlert(
			MDR_DIALECT_CHOICE ("New Messages","新着メッセージ"),
			text.String());
	}
	
	if (strategy & blink_leds)
		be_app->PostMessage('mblk');
	
	if (strategy & big_doozy_alert) {
		BMessage msg('numg');
		msg.AddInt32("num_messages",num_messages);
		msg.AddString("chain_name",chain->Name());
		msg.AddInt32("chain_id",chain->ID());
		
		be_app->PostMessage(&msg);
	}
}

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::ChainRunner *runner)
{
	return new NotifyFilter(settings,runner);
}

