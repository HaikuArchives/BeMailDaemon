#include <Application.h>
#include <Message.h>
#include <File.h>
#include <MessageRunner.h>
#include <Deskbar.h>
#include <Roster.h>
#include <Button.h>
#include <StringView.h>
#include <Mime.h>
#include <fs_index.h>
#include <fs_info.h>
#include <String.h>
#include <VolumeRoster.h>
#include <Query.h>
#include <NodeMonitor.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <MailSettings.h>
#include <status.h>

#include "deskbarview.h"
#include "LEDAnimation.h"

#ifdef BONE
	#ifdef _KERNEL_MODE
		#undef _KERNEL_MODE
		#include <sys/socket.h>
		#define _KERNEL_MODE 1
	#endif
	#include <bone_serial_ppp.h>
	#include <unistd.h>
#endif

StatusWindow *status;

class MailDaemonApp : public BApplication {
	public:
		MailDaemonApp(void);
		virtual ~MailDaemonApp();
		void MessageReceived(BMessage *msg);
		void InstallDeskbarIcon();
		void RemoveDeskbarIcon();
		void Pulse();
		bool QuitRequested();
		
		void SendPendingMessages();
		void GetNewMessages();
	private:
		void UpdateAutoCheck(bigtime_t interval);

		BMessageRunner *auto_check;
		MailSettings settings_file;
		
		int32 new_messages;
		BList fetch_done_respondents;
		
		BQuery *query;
		LEDAnimation *led;
		
		BString alert_string;
};


void makeIndices()
{
	const char *stringIndices[] = {	"MAIL:account","MAIL:cc","MAIL:draft","MAIL:flags",
									"MAIL:from","MAIL:name","MAIL:priority","MAIL:reply",
									"MAIL:status","MAIL:subject","MAIL:to","MAIL:to","MAIL:thread",NULL};

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

		fs_create_index(device,"MAIL:when",B_INT32_TYPE,0);
		fs_create_index(device,"MAIL:chain",B_INT32_TYPE,0);
	}
}


int main (int argc, const char **argv) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i],"-E") == 0) {
			if (!MailSettings().DaemonAutoStarts())
				return 0;
		}
	}

	MailDaemonApp app;
	
	// Add MAIL:account attribute if necessary
	BMimeType email_mime_type("text/x-email");
	BMessage info;
	
	email_mime_type.GetAttrInfo(&info);

	bool ok = true;
	int32 cnt = 0;
	const char * result;
	 
	while (ok) {
		if (info.FindString("attr:name", cnt, &result) == B_OK) {
			cnt++;
			if (strcmp(result, "MAIL:account") == 0)
				ok = false;
		} else {	
			info.AddString ("attr:name"        , "MAIL:account");
			info.AddString ("attr:public_name" , "Account"     );
			info.AddInt32  ("attr:type"        , B_STRING_TYPE );
			info.AddBool   ("attr:editable"    , false         );
			info.AddBool   ("attr:viewable"	   , true		   );
			info.AddBool   ("attr:extra"	   , false		   );
			info.AddInt32  ("attr:alignment"   , 0             );
			info.AddInt32  ("attr:width"       , 20            );
					
			email_mime_type.SetAttrInfo(&info);
			ok = false;
		}
	}
	
	cnt = 0;
	ok =true;
	while (ok) {
		if (info.FindString("attr:name", cnt, &result) == B_OK) {
			cnt++;
			if (strcmp(result, "MAIL:thread") == 0)
				ok = false;
		} else {	
			info.AddString ("attr:name"        , "MAIL:thread");
			info.AddString ("attr:public_name" , "Thread"     );
			info.AddInt32  ("attr:type"        , B_STRING_TYPE );
			info.AddBool   ("attr:editable"    , false         );
			info.AddBool   ("attr:viewable"	   , true		   );
			info.AddBool   ("attr:extra"	   , false		   );
			info.AddInt32  ("attr:alignment"   , 0             );
			info.AddInt32  ("attr:width"       , 20            );
					
			email_mime_type.SetAttrInfo(&info);
			ok = false;
		}
	}
	
	makeIndices();

	be_app->Run();
	return 0;
}


MailDaemonApp::MailDaemonApp(void)
  : BApplication("application/x-vnd.Be-POST" /* mail daemon sig */ )
{	
	InstallDeskbarIcon();
	
	status = new StatusWindow(BRect(40,400,360,400),"Mail Status", settings_file.ShowStatusWindow());
	auto_check = NULL;
	UpdateAutoCheck(settings_file.AutoCheckInterval());

	BVolume boot;
	query = new BQuery;
	
	BVolumeRoster().GetBootVolume(&boot);
	query->SetTarget(this);
	query->SetVolume(&boot);
	query->PushAttr("MAIL:status");
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
	if (new_messages > 0)
		string << new_messages;
	else
		string << "No";
		
	string << " new messages.";
	
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
					break;
				} else {
					if (status.connection_status != BSPPP_CONNECTED) {
						break;
					}
				}
#else
				if (find_thread("tty_thread") <= 0)
					break;
#endif
			}
		case 'mbth':
			PostMessage('msnd');
		case 'mnow': // check messages
			GetNewMessages();
			break;
		case 'msnd': // send messages
			SendPendingMessages();
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
			alert_string << num_messages << " new message";
			if (num_messages > 1)
				alert_string << 's';
				
			alert_string << " for " << msg->FindString("chain_name") << '\n';
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
			if (new_messages > 0)
				string << new_messages;
			else
				string << "No";
				
			string << " new messages.";
			
			status->SetDefaultMessage(string.String());
			
			}
			break;
	}
	BApplication::MessageReceived(msg);
}

void MailDaemonApp::InstallDeskbarIcon() {
	BDeskbar deskbar;

	if(deskbar.HasItem( "mail_daemon" ) == false) {
		BRoster roster;
		entry_ref ref;
		
		status_t status = roster.FindApp( "application/x-vnd.Be-POST" , &ref);
		if (status)
		{
			fprintf(stderr, "Can't find application to tell deskbar: %s\n", strerror(status));
			return;
		}
		
		//DeskbarView *view = new DeskbarView(BRect(0,0,15,15));
		//status = deskbar.AddItem(view);
		status = deskbar.AddItem(&ref);
		if (status)
		{
			fprintf(stderr, "Can't add deskbar replicant: %s\n", strerror(status));
			return;
		}
	}
}

void MailDaemonApp::RemoveDeskbarIcon() {
	BDeskbar deskbar;
	if( deskbar.HasItem( "mail_daemon" ))
		deskbar.RemoveItem( "mail_daemon" );
}

bool MailDaemonApp::QuitRequested() {
	RemoveDeskbarIcon();
	return true;
}

void MailDaemonApp::GetNewMessages() {
	BList *list = new BList;
	settings_file.InboundChains(list);
	MailChain *chain;
	
	for (int32 i = 0; i < list->CountItems(); i++) {
		chain = (MailChain *)(list->ItemAt(i));
		
		chain->RunChain(status,true,true,true);
	}	
}

void MailDaemonApp::SendPendingMessages() {
	BList *list = new BList;
	settings_file.OutboundChains(list);
	MailChain *chain;

	for (int32 i = 0; i < list->CountItems(); i++) {
		chain = (MailChain *)(list->ItemAt(i));
		
		chain->RunChain(status,true,true,true);
	}
}

void MailDaemonApp::Pulse() {
	bigtime_t idle = idle_time();
#ifdef DEBUG
	printf("Idle Time: %ld\n", idle);
#endif		
	if (led->IsRunning() && (idle < 999999)) {
#ifdef DEBUG
		printf("Stopping LEDs...\n");
#endif
		led->Stop();
	}
}
