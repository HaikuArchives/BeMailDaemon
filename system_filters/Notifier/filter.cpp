#include <Message.h>
#include <String.h>
#include <Alert.h>
#include <Beep.h>
#include <Application.h>

#include <MailAddon.h>
#include <ChainRunner.h>

enum {
	do_beep = 1,
	alert = 2,
	blink_leds = 4
};

class NotifyCallback : public MailCallback {
	public:
		NotifyCallback (int32 notification_method, MailChain *us);
		virtual void Callback(MDStatus result);
		
		uint32 num_messages;
	private:
		MailChain *chain;
		int32 strategy;
};

class NotifyFilter: public MailFilter
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
: MailFilter(msg)
{
	ChainRunner *runner;
	msg->FindPointer("chain_runner",(void **)&runner);

	callback = new NotifyCallback(msg->FindInt32("notification_method"),runner->Chain());
	
	runner->RegisterProcessCallback(callback);
}

status_t NotifyFilter::InitCheck(BString* err){ return B_OK; }

MDStatus NotifyFilter::ProcessMailMessage(BPositionIO**, BEntry*, BMessage*, BPath*, BString*)
{
	callback->num_messages ++;
	
	return MD_OK;
}

NotifyCallback::NotifyCallback (int32 notification_method, MailChain *us) : 
	strategy(notification_method),
	chain(us),
	num_messages(0) {}

void NotifyCallback::Callback(MDStatus result) {
	if (strategy & do_beep)
		system_beep("New E-mail");
		
	if (strategy & alert) {
		BString msg;
		msg << "You have " << num_messages << " new " << ((num_messages > 1) ? "messages" : "message") << " for " << chain->Name() << ".";
		(new BAlert("new_messages",msg.String(),"OK"))->Go(NULL);
	}
	
	if (strategy & blink_leds) {
		BMessage msg('numg');
		msg.AddInt32("num_messages",num_messages);
		msg.AddString("chain_name",chain->Name());
		msg.AddInt32("chain_id",chain->ID());
		
		be_app->PostMessage(&msg);
	}
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView *view)
{
	return new NotifyFilter(settings);
}

