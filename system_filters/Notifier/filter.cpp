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

#include "ConfigView.h"

using namespace Zoidberg;


class NotifyCallback : public Mail::ChainCallback {
	public:
		NotifyCallback (int32 notification_method, Mail::Chain *us);
		virtual void Callback(MDStatus result);
		
		uint32 num_messages;
	private:
		Mail::Chain *chain;
		int32 strategy;
};

class NotifyFilter : public Mail::Filter
{
  public:
	NotifyFilter(BMessage*);
	virtual status_t InitCheck(BString *err);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
	
  private:
  	NotifyCallback *callback;
};


NotifyFilter::NotifyFilter(BMessage* msg)
	: Mail::Filter(msg)
{
	Mail::ChainRunner *runner;
	msg->FindPointer("chain_runner",(void **)&runner);

	callback = new NotifyCallback(msg->FindInt32("notification_method"),runner->Chain());
	
	runner->RegisterProcessCallback(callback);
}

status_t NotifyFilter::InitCheck(BString* err)
{
	return B_OK;
}

MDStatus NotifyFilter::ProcessMailMessage(BPositionIO**, BEntry*, BMessage*, BPath*, BString*)
{
	callback->num_messages ++;
	
	return MD_OK;
}

NotifyCallback::NotifyCallback (int32 notification_method, Mail::Chain *us) : 
	strategy(notification_method),
	chain(us),
	num_messages(0)
{
}

void NotifyCallback::Callback(MDStatus result) {
	if (strategy & do_beep)
		system_beep("New E-mail");
		
	if (strategy & alert) {
		BString text;
		text << "You have " << num_messages << " new message" << ((num_messages != 1) ? "s" : "")
			 << " for " << chain->Name() << ".";
		Mail::ShowAlert("New Messages", text.String());
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

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView *view)
{
	return new NotifyFilter(settings);
}

