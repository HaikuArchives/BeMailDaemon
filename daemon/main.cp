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

#ifdef BONE
#	include <bone_serial_ppp.h>
#	include <unistd.h>
#endif


StatusWindow *status;

class MailDaemon : public BApplication {
	public:
		MailDaemon(void);
		virtual ~MailDaemon();
		void MessageReceived(BMessage *msg);
		void InstallDeskbarIcon();
		void RemoveDeskbarIcon();
		bool QuitRequested();
		
		void SendPendingMessages();
		void GetNewMessages();
	private:
		BMessageRunner *auto_check;
		MailSettings settings_file;
		
		int32 new_messages;
		BList clean_up;
};


void makeIndices()
{
	const char *stringIndices[] = {	"MAIL:account","MAIL:cc","MAIL:draft","MAIL:flags",
									"MAIL:from","MAIL:name","MAIL:priority","MAIL:reply",
									"MAIL:status","MAIL:subject","MAIL:to","MAIL:to",NULL};

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

	MailDaemon app;
	
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
	makeIndices();

	be_app->Run();
	return 0;
}


MailDaemon::MailDaemon(void)
  : BApplication("application/x-vnd.Be-POST" /* mail daemon sig */ )
{	
	InstallDeskbarIcon();
	
	status = new StatusWindow(BRect(40,400,360,400),"Mail Status", settings_file.ShowStatusWindow());
	auto_check = new BMessageRunner(be_app_messenger,new BMessage('moto'),settings_file.AutoCheckInterval());

	BVolume boot;
	BQuery *query = new BQuery;
	
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
	
	status->SetDefaultMessage(strdup(string.String())); /* Memory Leak!!!!!! How to fix? */
}

MailDaemon::~MailDaemon()
{
	delete auto_check;
}

void MailDaemon::MessageReceived(BMessage *msg) {
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
			auto_check->SetInterval(settings_file.AutoCheckInterval());
			status->SetShowCriterion(settings_file.ShowStatusWindow());
			break;
		case 'lkch':
			status->PostMessage(msg);
			break;
		case 'stwg': //---StaT Window Gone
			{
			MailChain *chain;
			while ((chain = (MailChain *)clean_up.RemoveItem(0L)) != NULL) {
				chain->Save();
				delete chain;
			}
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
		default:
			BApplication::MessageReceived(msg);
	}
}

void MailDaemon::InstallDeskbarIcon() {
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
		
		DeskbarView *view = new DeskbarView(BRect(0,0,15,15));
		status = deskbar.AddItem(view);
		if (status)
		{
			fprintf(stderr, "Can't add deskbar replicant: %s\n", strerror(status));
			return;
		}
	}
}

void MailDaemon::RemoveDeskbarIcon() {
	BDeskbar deskbar;
	if( deskbar.HasItem( "mail_daemon" ))
		deskbar.RemoveItem( "mail_daemon" );
}

bool MailDaemon::QuitRequested() {
	RemoveDeskbarIcon();
	return true;
}

void MailDaemon::GetNewMessages() {
	BList *list = new BList;
	settings_file.InboundChains(list);
	MailChain *chain;
	
	for (int32 i = 0; i < list->CountItems(); i++) {
		chain = (MailChain *)(list->ItemAt(i));
		
		chain->RunChain(status);
	}
	
	clean_up.AddList(list);
}

void MailDaemon::SendPendingMessages() {
	BList *list = new BList;
	settings_file.OutboundChains(list);
	MailChain *chain;
	
	for (int32 i = 0; i < list->CountItems(); i++) {
		chain = (MailChain *)(list->ItemAt(i));
		
		chain->RunChain(status);
	}
	
	clean_up.AddList(list);
}