/* main - the daemon's inner workings
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Application.h>
#include <Message.h>
#include <File.h>
#include <MessageRunner.h>
#include <Deskbar.h>
#include <Roster.h>
#include <Button.h>
#include <StringView.h>
#include <Mime.h>
#include <Beep.h>
#include <fs_index.h>
#include <fs_info.h>
#include <String.h>
#include <VolumeRoster.h>
#include <Query.h>
#include <NodeMonitor.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <E-mail.h>
#include <MailSettings.h>
#include <MailMessage.h>
#include <status.h>

#include "deskbarview.h"
#include "LEDAnimation.h"

#include <MDRLanguage.h>

#ifdef BONE
	#ifdef _KERNEL_MODE
		#undef _KERNEL_MODE
		#include <sys/socket.h>
		#define _KERNEL_MODE 1
	#endif
	#include <bone_serial_ppp.h>
	#include <unistd.h>
#endif


namespace Zoidberg {
namespace Mail {

StatusWindow *status;

class MailDaemonApp : public BApplication {
	public:
		MailDaemonApp(void);
		virtual ~MailDaemonApp();

		virtual void MessageReceived(BMessage *msg);
		virtual void Pulse();
		virtual bool QuitRequested();
		virtual void ReadyToRun();

		void InstallDeskbarIcon();
		void RemoveDeskbarIcon();
		
		void RunChains(BList &list,BMessage *msg);
		void SendPendingMessages(BMessage *msg);
		void GetNewMessages(BMessage *msg);

	private:
		void UpdateAutoCheck(bigtime_t interval);

		BMessageRunner *auto_check;
		Settings settings_file;
		
		int32 new_messages;
		BList fetch_done_respondents;
		
		BQuery *query;
		LEDAnimation *led;
		
		BString alert_string;
};


MailDaemonApp::MailDaemonApp(void)
	: BApplication("application/x-vnd.Be-POST" /* mail daemon sig */ )
{
	InstallDeskbarIcon();
	
	status = new StatusWindow(BRect(40,400,360,400),"Mail Status", settings_file.ShowStatusWindow());
	auto_check = NULL;
}

void MailDaemonApp::ReadyToRun() {
	UpdateAutoCheck(settings_file.AutoCheckInterval());

	BVolume boot;
	query = new BQuery;
	
	BVolumeRoster().GetBootVolume(&boot);
	query->SetTarget(this);
	query->SetVolume(&boot);
	query->PushAttr(B_MAIL_ATTR_STATUS);
	query->PushString("New");
	query->PushOp(B_EQ);
	query->PushAttr("BEOS:TYPE");
	query->PushString("text/x-email");
	query->PushOp(B_EQ);
	query->PushOp(B_AND);
	query->Fetch();
	
	BEntry entry;
	for (new_messages = 0; query->GetNextEntry(&entry) == B_OK; new_messages++);
	
	BString string;
	MDR_DIALECT_CHOICE (
		if (new_messages > 0)
			string << new_messages;
		else
			string << "No";
		string << " new messages.";,
		if (new_messages > 0)
			string << new_messages << " 通の未読メッセージがあります ";
		else
			string << "未読メッセージはありません";
	);	
	
	status->SetDefaultMessage(string);
	
	led = new LEDAnimation;
	SetPulseRate(1000000);	
}

MailDaemonApp::~MailDaemonApp()
{
	delete auto_check;
	delete query;
	delete led;
}


void MailDaemonApp::UpdateAutoCheck(bigtime_t interval)
{
	if (interval > 0)
	{
		if (auto_check != NULL)
		{
			auto_check->SetInterval(interval);
			auto_check->SetCount(-1);
		}
		else
			auto_check = new BMessageRunner(be_app_messenger,new BMessage('moto'),interval);
	}
	else
	{
		delete auto_check;
		auto_check = NULL;
	}
}


void MailDaemonApp::MessageReceived(BMessage *msg) {
	switch (msg->what) {
		case 'moto':
			if (settings_file.CheckOnlyIfPPPUp()) {
#ifdef BONE
				int s = socket(AF_INET, SOCK_DGRAM, 0);
				bsppp_status_t status;
			
				strcpy(status.if_name, "ppp0");
				if (ioctl(s, BONE_SERIAL_PPP_GET_STATUS, &status, sizeof(status)) != 0) {
					close(s);
					break;
				} else {
					if (status.connection_status != BSPPP_CONNECTED) {
						close(s);
						break;
					}
				}
				close (s);
#else
				if (find_thread("tty_thread") <= 0)
					break;
#endif
			}
			// supposed to fall through
		case 'mbth':	// check & send messages
			msg->what = 'msnd';
			PostMessage(msg); //'msnd');
			// supposed to fall trough
		case 'mnow':	// check messages
			GetNewMessages(msg);
			break;
		case 'msnd':	// send messages
			SendPendingMessages(msg);
			break;

		case 'mrrs':
			settings_file.Reload();
			UpdateAutoCheck(settings_file.AutoCheckInterval());
			status->SetShowCriterion(settings_file.ShowStatusWindow());
			break;
		case 'shst':	// when to show the status window
		{
			int32 mode;
			if (msg->FindInt32("ShowStatusWindow",&mode) == B_OK)
				status->SetShowCriterion(mode);
			break;
		}
		case 'lkch':	// status window look changed
		case 'wsch':	// workspace changed
			status->PostMessage(msg);
			break;
		case 'stwg': //----StaT Window Gone
			{
			BMessage *msg, reply('mnuc');
			reply.AddInt32("num_new_messages",new_messages);
			
			while(msg = (BMessage *)fetch_done_respondents.RemoveItem(0L)) {
				msg->SendReply(&reply);
				delete msg;
			}
			}
			if (alert_string != B_EMPTY_STRING) {
				alert_string.Truncate(alert_string.Length()-1);
				ShowAlert("New Messages",alert_string.String());
				alert_string = B_EMPTY_STRING;
			}
			break;
		case 'mnum': //----Number of new messages
			{
			BMessage reply('mnuc' /* Mail NU message Count */);
			if (msg->FindBool("wait_for_fetch_done")) {
				fetch_done_respondents.AddItem(DetachCurrentMessage());
				break;
			}
			
			reply.AddInt32("num_new_messages",new_messages);
			msg->SendReply(&reply);
			}
			break;
		case 'mblk': //-----Mail BLinK
			if (new_messages > 0)
				led->Start();
			break;
		case 'numg':
			{
			int32 num_messages = msg->FindInt32("num_messages");
			MDR_DIALECT_CHOICE (
				alert_string << num_messages << " new message";
				if (num_messages > 1)
					alert_string << 's';
	
				alert_string << " for " << msg->FindString("chain_name") << '\n';,

				alert_string << msg->FindString("chain_name") << "より\n" << num_messages
					<< " 通のメッセージが届きました";
			);
			
			}
			break;
		case B_QUERY_UPDATE:
			{
			int32 what;
			msg->FindInt32("opcode",&what);
			switch (what) {
				case B_ENTRY_CREATED:
					new_messages++;
					break;
				case B_ENTRY_REMOVED:
					new_messages--;
					break;
			}
			
			BString string;
			
			MDR_DIALECT_CHOICE (
				if (new_messages > 0)
					string << new_messages;
				else
					string << "No";
				string << " new messages.";,
				if (new_messages > 0)
					string << new_messages << " 通の未読メッセージがあります";
				else
					string << "未読メッセージはありません";
			);
			
			status->SetDefaultMessage(string.String());
			
			}
			break;
	}
	BApplication::MessageReceived(msg);
}


void
MailDaemonApp::InstallDeskbarIcon()
{
	BDeskbar deskbar;

	if (!deskbar.HasItem("mail_daemon")) {
		BRoster roster;
		entry_ref ref;
		
		status_t status = roster.FindApp("application/x-vnd.Be-POST", &ref);
		if (status < B_OK) {
			fprintf(stderr, "Can't find application to tell deskbar: %s\n", strerror(status));
			return;
		}

		status = deskbar.AddItem(&ref);
		if (status < B_OK) {
			fprintf(stderr, "Can't add deskbar replicant: %s\n", strerror(status));
			return;
		}
	}
}


void
MailDaemonApp::RemoveDeskbarIcon()
{
	BDeskbar deskbar;
	if (deskbar.HasItem("mail_daemon"))
		deskbar.RemoveItem("mail_daemon");
}


bool
MailDaemonApp::QuitRequested()
{
	RemoveDeskbarIcon();
	return true;
}


void
MailDaemonApp::RunChains(BList &list,BMessage *msg)
{
	Chain *chain;

	int32 index = 0,id;
	for (;msg->FindInt32("chain",index,&id) == B_OK;index++)
	{
		for (int32 i = 0; i < list.CountItems(); i++) {
			chain = (Chain *)list.ItemAt(i);
			
			if (chain->ID() == id)
			{
				chain->RunChain(status,true,true,true);
				list.RemoveItem(i);	// the chain runner deletes the chain
				break;
			}
		}
	}
	
	if (index == 0)	// invoke all chains
	{
		for (int32 i = 0; i < list.CountItems(); i++) {
			chain = (Chain *)list.ItemAt(i);
			
			chain->RunChain(status,true,true,true);
		}
	}
	else	// delete unused chains
	{
		for (int32 i = list.CountItems();i-- > 0;)
			delete (Chain *)list.RemoveItem(i);
	}
}

void MailDaemonApp::GetNewMessages(BMessage *msg) {
	BList list;
	InboundChains(&list);

	RunChains(list,msg);	
}

void MailDaemonApp::SendPendingMessages(BMessage *msg) {
	BList list;
	OutboundChains(&list);

	RunChains(list,msg);
}

void MailDaemonApp::Pulse() {
	bigtime_t idle = idle_time();
	if (led->IsRunning() && (idle < 100000))
		led->Stop();
}

}	// namespace Mail
}	// namespace Zoidberg

//	#pragma mark -


void makeIndices()
{
	const char *stringIndices[] = {	B_MAIL_ATTR_ACCOUNT,B_MAIL_ATTR_CC,
									B_MAIL_ATTR_FLAGS,B_MAIL_ATTR_FROM,B_MAIL_ATTR_NAME,
									B_MAIL_ATTR_PRIORITY,B_MAIL_ATTR_REPLY,B_MAIL_ATTR_STATUS,
									B_MAIL_ATTR_SUBJECT,B_MAIL_ATTR_TO,B_MAIL_ATTR_THREAD, NULL};

	// add mail indices for all devices capable of querying

	int32 cookie = 0;
	dev_t device;
	while ((device = next_dev(&cookie)) >= B_OK)
	{
		fs_info info;
		if (fs_stat_dev(device,&info) < 0 || (info.flags & B_FS_HAS_QUERY) == 0)
			continue;

		int32 i = 0;
		for (;stringIndices[i];i++)
			fs_create_index(device,stringIndices[i],B_STRING_TYPE,0);

		fs_create_index(device,"MAIL:draft", B_INT32_TYPE, 0);
		fs_create_index(device,B_MAIL_ATTR_WHEN,B_INT32_TYPE,0);
		fs_create_index(device,"MAIL:chain",B_INT32_TYPE,0);
	}
}


void
addAttribute(BMessage &msg,char *name,char *publicName,int32 type = B_STRING_TYPE,bool viewable = true,bool editable = false,int32 width = 200)
{
	msg.AddString("attr:name",name); 
	msg.AddString("attr:public_name",publicName); 
	msg.AddInt32("attr:type",type); 
	msg.AddBool("attr:viewable",viewable); 
	msg.AddBool("attr:editable",editable);
	msg.AddInt32("attr:width",width);
	msg.AddInt32("attr:alignment",B_ALIGN_LEFT); 
}


void
makeMimeType()
{
	// Add MAIL:account attribute if necessary
	BMimeType mime("text/x-email");

	if (mime.InitCheck() != B_OK) {
		fputs("could not init mime type.\n",stderr);
		return;
	}

	BMessage info;
	mime.GetAttrInfo(&info);

	if (!mime.IsInstalled()) {
		// install the full mime type

		mime.Install();

		addAttribute(info,B_MAIL_ATTR_NAME,"Name");
		addAttribute(info,B_MAIL_ATTR_SUBJECT,"Subject");
		addAttribute(info,B_MAIL_ATTR_TO,"To");
		addAttribute(info,B_MAIL_ATTR_CC,"Cc");
		addAttribute(info,B_MAIL_ATTR_FROM,"From");
		addAttribute(info,B_MAIL_ATTR_REPLY,"Reply To");
		addAttribute(info,B_MAIL_ATTR_STATUS,"Status");
		addAttribute(info,B_MAIL_ATTR_PRIORITY,"Priority",B_STRING_TYPE,true,true,40);
		addAttribute(info,B_MAIL_ATTR_WHEN,"When",B_TIME_TYPE,true,false,150);
		addAttribute(info,B_MAIL_ATTR_THREAD,"Thread");
		addAttribute(info,B_MAIL_ATTR_ACCOUNT,"Account",B_STRING_TYPE,true,false,100);

		mime.SetAttrInfo(&info);
		mime.SetShortDescription("E-mail");
		mime.SetPreferredApp("application/x-vnd.Be-MAIL");
	} else {
		// just add the types we add to the system

		bool hasAccount = false, hasThread = false;
		const char *result;
		for (int32 index = 0;info.FindString("attr:name",index,&result) == B_OK;index++) {
			if (!strcmp(result,B_MAIL_ATTR_ACCOUNT))
				hasAccount = true;
			if (!strcmp(result,B_MAIL_ATTR_THREAD))
				hasThread = true;
		}
	
		if (!hasAccount)
			addAttribute(info,B_MAIL_ATTR_ACCOUNT,"Account",B_STRING_TYPE,true,false,100);
		if (!hasThread)
			addAttribute(info,B_MAIL_ATTR_THREAD,"Thread");

		if (!hasAccount || !hasThread)
			mime.SetAttrInfo(&info);
	}
}


int main (int argc, const char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i],"-E") == 0) {
			if (!Zoidberg::Mail::Settings().DaemonAutoStarts())
				return 0;
		}
	}

	Zoidberg::Mail::MailDaemonApp app;

	// install MimeTypes, attributes, indices, and the
	// system beep add startup

	makeMimeType();	
	makeIndices();
	add_system_beep_event("New E-mail");

	be_app->Run();
	return 0;
}

