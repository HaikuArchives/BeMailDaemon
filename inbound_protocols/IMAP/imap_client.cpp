#include <MailProtocol.h>
#include <NetEndpoint.h>
#include <Message.h>
#include <ChainRunner.h>
#include <MDRLanguage.h>
#include <Debug.h>
#include <StringList.h>
#include <Entry.h>
#include <Directory.h>
#include <File.h>
#include <MessageRunner.h>
#include <Path.h>
#include <crypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "NestedString.h"

using namespace Zoidberg;

#define CRLF "\r\n"
#define xEOF    236
const bigtime_t kIMAP4ClientTimeout = 1000000*60; // 60 sec

#define DEBUG 1

enum { OK,BAD,NO,CONTINUE, NOT_COMMAND_RESPONSE };

class IMAP4Client : public Mail::Protocol {
	public:
		IMAP4Client(BMessage *settings, Mail::ChainRunner *run);
		virtual ~IMAP4Client();
		
		virtual status_t GetMessage(
			const char* uid,
			BPositionIO** out_file, BMessage* out_headers,
			BPath* out_folder_location);
			
		virtual status_t DeleteMessage(const char* uid);
		
		status_t ReceiveLine(BString &out);
		status_t SendCommand(const char *command);
		
		status_t Select(const char *mb);
		
		virtual status_t InitCheck(BString *) { return B_OK; }
		
		int GetResponse(BString &tag, NestedString *parsed_response, bool report_literals = false, bool recursion_flag = false);
		bool WasCommandOkay(BString &response);
		
		void InitializeMailboxes();
		void SyncAllBoxes();
	
	private:
		friend class SyncHandler;
		friend class SyncCallback;
		friend class NoopWorker;
		friend class IMAP4PartialReader;
		
		SyncHandler *sync;
		NoopWorker *noop;
		BMessageRunner *nooprunner;
		
		int32 commandCount;
		BNetEndpoint *net;
		BString selected_mb, inbox_name;
		StringList boxes;
		status_t err;
};

class SyncHandler;

class SyncCallback : public Mail::ChainCallback {
	public:
		SyncCallback(SyncHandler *a) : handler(a) {}
		void Callback(status_t);
		
	private:
		SyncHandler *handler;
};

class SyncHandler : public BHandler {
	public:
		SyncHandler(IMAP4Client *us) : BHandler(), client(us) {}
		~SyncHandler() { delete runner; }
		void MessageReceived(BMessage *msg) {
			delete runner;
			if (msg->what != 'imps' /*IMaP Sync */)
				return;
			
			StringList list;
			list += "//!newmsgcheck";
			client->runner->RegisterProcessCallback(new SyncCallback(this));
			client->runner->GetMessages(&list,0);
		}
	
		BMessageRunner *runner;
		IMAP4Client *client;
};

void SyncCallback::Callback(status_t) {
	handler->runner = new BMessageRunner(BMessenger(handler,handler->client->runner),new BMessage('imps'),30e6,1);
}

class NoopWorker : public BHandler {
	public:
		NoopWorker(IMAP4Client *a) : us(a) {}
		void RecentMessages() {
			int32 num_messages = -1;
			int32 total = -1;
			char expected[255];
			BString tag;
			::sprintf(expected,"a%.7ld",us->commandCount);
			while(1) {
				NestedString response;
				us->GetResponse(tag,&response);
				
				if (tag == expected)
					break;
				
				if (strcasecmp(response[1](),"RECENT") == 0)
					num_messages = atoi(response[0]());
				if (strcasecmp(response[1](),"EXISTS") == 0)
					total = atoi(response[0]());
			}
			if ((num_messages <= 0) || (total <= 0))
				return;
				
			BString command;
			command = "FETCH ";
			command << total - num_messages + 1 << ':' << total << " UID";
			us->SendCommand(command.String());
			::sprintf(expected,"a%.7ld",us->commandCount);
			StringList list;
			BString uid;
			while(1) {
				NestedString response;
				us->GetResponse(tag,&response);
							
				if (tag == expected)
					break;
				
				uid = us->inbox_name;
				uid << '/' << response[2][1]();
				list.AddItem(uid.String());
			}
			
			(*us->unique_ids) += list;
			us->runner->GetMessages(&list,-1);
		}
		void MessageReceived(BMessage *msg) {
			if (msg->what != 'impn' /* IMaP Noop */)
				return;
				
			if (strcasecmp(us->selected_mb.String(),"INBOX")) {
				us->SendCommand("CLOSE");
				BString blork;
				us->WasCommandOkay(blork);
				us->selected_mb = "";
			}
			
			if (us->selected_mb == "") {
				us->SendCommand("SELECT INBOX");
				us->selected_mb = us->inbox_name;
				RecentMessages();
			} else {
				us->SendCommand("NOOP");
				RecentMessages();
			}
		}
	private:
		IMAP4Client *us;
};

IMAP4Client::IMAP4Client(BMessage *settings, Mail::ChainRunner *run) : Mail::Protocol(settings,run), commandCount(0), net(NULL), selected_mb("") {
	err = B_OK;
		
	net = new BNetEndpoint;
	int port = settings->FindInt32("port");
	if (port <= 0)
		port = 143;

//-----Open TCP link	
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Opening connection...","接続中..."));
	err = net->Connect(settings->FindString("server"),port);
	if (err < B_OK) {
		BString error;
		error << "IMAP Server " << settings->FindString("server");
		if (port != 143)
			error << ":" << port;
		error << " Unreachable";
		runner->ShowError(error.String());
		return;
	}

//-----Wait for welcome message
	BString response;
	ReceiveLine(response);

//-----Log in
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Authenticating...","認証中..."));
	
	const char *password = settings->FindString("password");
	{
		char *passwd = get_passwd(settings,"cpasswd");
		if (passwd)
			password = passwd;
	}

	BString command = "LOGIN ";
	command << "\"" << settings->FindString("username") << "\" ";
	command << "\"" << password << "\"";
	SendCommand(command.String());
	if (!WasCommandOkay(response)) {
		response.Prepend("Error during login: ");
		runner->ShowError(response.String());
		return;
	}
	
	runner->ReportProgress(0,0,"Logged in");
	
	InitializeMailboxes();
	SyncAllBoxes();
	StringList fake;
	fake += "";
	runner->GetMessages(&fake,-1);
	
	sync = new SyncHandler(this);
	runner->AddHandler(sync);
	noop = new NoopWorker(this);
	runner->AddHandler(noop);
	sync->runner = new BMessageRunner(BMessenger(sync,runner),new BMessage('imps'),300e6,1);
	nooprunner = new BMessageRunner(BMessenger(noop,runner),new BMessage('impn'),10e6);
}

IMAP4Client::~IMAP4Client() {	
	if (selected_mb != "")
		SendCommand("CLOSE");
	SendCommand("LOGOUT");
	
	delete sync;
	delete noop;
}

void IMAP4Client::InitializeMailboxes() {
	SendCommand("LIST \"\" \"*\"");
	BString tag;
	char expected[255];
	::sprintf(expected,"a%.7ld",commandCount);
	create_directory(runner->Chain()->MetaData()->FindString("path"),0777);
	BDirectory dir(runner->Chain()->MetaData()->FindString("path"));
	BDirectory trash;
	int val;
	do {
		NestedString response;
		val = GetResponse(tag,&response);
		if (val != NOT_COMMAND_RESPONSE)
			break;
				
		if (tag == expected)
			break;
		
		if (response[3]()[0] != '.') {
			boxes += response[3]();
			if (strcasecmp(response[3](),"INBOX") == 0)
				inbox_name = response[3]();
				
			dir.CreateDirectory(response[3](),&trash);
		}
	} while (1);
	
	/*puts("Mailboxes:");
	for (int32 i = 0; i < boxes.CountItems(); i++)
		printf("\t%s\n",boxes[i]);*/
}

#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");

void GetSubFolders(BDirectory *of, StringList *folders, const char *prepend);

void GetSubFolders(BDirectory *of, StringList *folders, const char *prepend) {
	of->Rewind();
	BEntry ent;
	BString crud;
	BDirectory sub;
	char buf[255];
	while (of->GetNextEntry(&ent) == B_OK) {
		if (ent.IsDirectory()) {
			sub.SetTo(&ent);
			ent.GetName(buf);
			crud = prepend;
			crud << buf << '/';
			GetSubFolders(&sub,folders,crud.String());
			crud = prepend;
			crud << buf;
			(*folders) += crud.String();
		}
	}
}
	
	
void IMAP4Client::SyncAllBoxes() {
	runner->ReportProgress(0,0,"Synchronizing Mailboxes");
	BString command;
	char expected[255];
	StringList *list = new StringList;
	BString uid,tag;
	
	if (selected_mb != "") {
		SendCommand("CLOSE");
		WasCommandOkay(command);
		selected_mb = "";
	}
	
	StringList folders;
	BEntry entry;
	BDirectory dir(runner->Chain()->MetaData()->FindString("path"));
	GetSubFolders(&dir,&folders,"");
	StringList temp;
	folders.NotThere(boxes,&temp);

	for (int32 i = 0; i < temp.CountItems(); i++) {
		command = "";
		command << "CREATE \"" << temp[i] << '\"';
		
		boxes += temp[i];
		
		SendCommand(command.String());
		if (!WasCommandOkay(command)) {
			command = "Error creating mailbox ";
			command << temp[i] << '.';
			runner->ShowError(command.String());
		}
	}		
			
	for (int32 i = 0; i < boxes.CountItems(); i++) {
		int32 num_messages = -1;
		
		command = "SELECT \"";
		command << boxes[i] << '\"';
		
		SendCommand(command.String());
		::sprintf(expected,"a%.7ld",commandCount);
		selected_mb = boxes[i];
		while(1) {
			NestedString response;
			GetResponse(tag,&response);
			
			//response.PrintToStream();
			
			if (tag == expected)
				break;
			
			if (strcasecmp(response[1](),"EXISTS") == 0)
				num_messages = atoi(response[0]());
		}
		if (num_messages < 0) {
			BString error = "Error while opening mailbox: ";
			error << boxes[i];
			runner->ShowError(error.String());
			delete list;
			return;
		}
		if (num_messages == 0) {
			SendCommand("CLOSE");
			WasCommandOkay(uid);
			selected_mb = "";
			continue;
		}
		
		command = "FETCH 1:";
		command << num_messages << " UID";
		SendCommand(command.String());
		::sprintf(expected,"a%.7ld",commandCount);
		while(1) {
			NestedString response;
			GetResponse(tag,&response);
						
			if (tag == expected)
				break;
			
			uid = boxes[i];
			uid << '/' << response[2][1]();
			list->AddItem(uid.String());
		}
		
		SendCommand("CLOSE");
		WasCommandOkay(uid);
		selected_mb = "";
	}
	
	delete unique_ids;
	unique_ids = list;
	
	StringList to_dl;
	unique_ids->NotThere(*manifest,&to_dl);
	
	if (to_dl.CountItems() > 0)
		runner->GetMessages(&to_dl,-1);
	
	folders.MakeEmpty();
	temp.MakeEmpty();
	GetSubFolders(&dir,&folders,"");
	folders.NotHere(boxes,&temp);
	for (int32 i = 0; i < temp.CountItems(); i++) {
		command = "";
		command << "DELETE \"" << temp[i] << '\"';
		
		boxes -= temp[i];
		
		SendCommand(command.String());
		if (!WasCommandOkay(command)) {
			command = "Error creating mailbox ";
			command << temp[i] << '.';
			runner->ShowError(command.String());
		}
	}
	
	BDirectory folder;
	BString string;
	BFile snoodle;
	int32 chain;
	bool append;
	for (int32 i = 0; i < boxes.CountItems(); i++) {
		dir.FindEntry(boxes[i],&entry);
		folder.SetTo(&entry);
		folder.Rewind();
		while (folder.GetNextEntry(&entry) == B_OK) {
			if (!entry.IsFile())
				continue;
			snoodle.SetTo(&entry,B_READ_WRITE);
			append = false;
			if (snoodle.ReadAttr("MAIL:chain",B_INT32_TYPE,0,&chain,sizeof(chain)) < B_OK)
				append = true;
			if (chain != runner->Chain()->ID())
				append = true;
			if (snoodle.ReadAttrString("MAIL:unique_id",&string) < B_OK)
				append = true;
			if (strncmp(string.String(),boxes[i],strlen(boxes[i])) == 0)
				continue;
			
			if (!append) {
				BString mb(string), id;
				mb.Truncate(string.FindLast('/'));
				string.CopyInto(id,string.FindLast('/') + 1,string.Length());
								
				Select(mb.String());
				
				command = "UID COPY ";
				command << id << " \"" << boxes[i] << '\"';
				SendCommand(command.String());
				WasCommandOkay(command);
			} else {
				if (selected_mb != "") {
					BString trash;
					SendCommand("CLOSE");
					WasCommandOkay(trash);
					selected_mb = "";
				}
				command = "APPEND \"";
				off_t size;
				snoodle.GetSize(&size);
				command << boxes[i] << "\" {" << size << '}';
				SendCommand(command.String());
				ReceiveLine(command);
				char *buffer = new char[size];
				snoodle.ReadAt(0,buffer,size);
				net->Send(buffer,size);
				net->Send("\r\n",2);
				WasCommandOkay(command);
			}
			
			if (selected_mb != boxes[i]) {
				if (selected_mb != "") {
					BString trash;
					SendCommand("CLOSE");
					WasCommandOkay(trash);
					selected_mb = "";
				}
				BString cmd = "SELECT \"";
				cmd << boxes[i] << '\"';
				selected_mb = boxes[i];
				SendCommand(cmd.String());
			} else {
				SendCommand("NOOP");
			}
			
			::sprintf(expected,"a%.7ld",commandCount);
			
			//----This is a race condition, but we've got no choice
			int32 exists;
			
			while(1) {
				NestedString response;
				GetResponse(tag,&response);
				
				if (tag == expected)
					break;
				
				if (strcasecmp(response[1](),"EXISTS") == 0)
					exists = atoi(response[0]());
			}
				
			BString command;
			command = "FETCH ";
			command << exists << " UID";
			SendCommand(command.String());
			::sprintf(expected,"a%.7ld",commandCount);
			BString uid;
			while (1) {
				NestedString response;
				GetResponse(tag,&response);
							
				if (tag == expected)
					break;
				
				uid = boxes[i];
				uid << '/' << response[2][1]();
			}
			
			snoodle.WriteAttrString("MAIL:unique_id",&uid);
			(*manifest) += uid.String();
			(*unique_ids) += uid.String();
			chain = runner->Chain()->ID();
			snoodle.WriteAttr("MAIL:chain",B_INT32_TYPE,0,&chain,sizeof(chain));
		}
	}
}

status_t IMAP4Client::Select(const char *mb) {
	if (selected_mb != mb) {
		if (selected_mb != "") {
			BString trash;
			SendCommand("CLOSE");
			WasCommandOkay(trash);
			selected_mb = "";
		}
		BString cmd = "SELECT \"";
		cmd << mb << '\"';
		SendCommand(cmd.String());
		
		if (!WasCommandOkay(cmd)) {
			runner->ShowError(cmd.String());
			return B_ERROR;
		}
		
		selected_mb = mb;
	}
		
	return B_OK;
}

class IMAP4PartialReader : public BPositionIO {
	public:
		IMAP4PartialReader(IMAP4Client *client,BPositionIO *_slave,const char *id) : us(client), slave(_slave), parts_read(0) {
			strcpy(unique,id);
		}
		~IMAP4PartialReader() {
			delete slave;
			us->runner->ReportProgress(0,1);
		}
		off_t Seek(off_t position, uint32 seek_mode) {
			ssize_t errorCode;
			char    tempBuffer [1];
			
			if (seek_mode == SEEK_END) {
				switch (parts_read) {
					case 0:
						slave->Seek(0,SEEK_SET);
						FetchMessage("RFC822");
						break;
					case 1:
						slave->Seek(0,SEEK_END);
						FetchMessage("RFC822.TEXT");
						break;
				}
				parts_read = 2;
			}
			return slave->Seek(position,seek_mode);
		}
		off_t Position() const {
			return slave->Position();
		}
		ssize_t	WriteAt(off_t pos, const void *buffer, size_t amountToWrite) {
			return slave->WriteAt(pos,buffer,amountToWrite);
		}
		ssize_t	ReadAt(off_t pos, void *buffer, size_t amountToWrite) {
			ssize_t bytes;
			while ((bytes = slave->ReadAt(pos,buffer,amountToWrite)) < amountToWrite && parts_read < 2) {
				 switch (parts_read++) {
					case 0:
						slave->Seek(0,SEEK_SET);
						FetchMessage("RFC822.HEADER");
						//Write("\r\n",2);
						break;
					case 1:
						slave->Seek(0,SEEK_END);
						FetchMessage("RFC822.TEXT");
						break;
				}
			}
			return bytes;
		}
	private:
		void FetchMessage(const char *part) {
			BString command = "UID FETCH ";
			command << unique << " (" << part << ')';
			us->SendCommand(command.String());
			static char cmd[255];
			::sprintf(cmd,"a%.7ld"CRLF,us->commandCount);
			NestedString response;
			if (us->GetResponse(command,&response) != NOT_COMMAND_RESPONSE && command == cmd)
				return;
			us->WasCommandOkay(command);
			slave->Write(response[2][3](),strlen(response[2][3]()));
		}
			
		IMAP4Client *us;
		char unique[25];
		BPositionIO *slave;
		int parts_read;
};
		

status_t IMAP4Client::GetMessage(
	const char* uid, BPositionIO** out_file, BMessage* out_headers,
	BPath* out_folder_location) {
			if (strcmp("//!newmsgcheck",uid) == 0) {
				SyncAllBoxes();
				return B_MAIL_END_FETCH;
			}
			
			if (uid[0] == 0)
				return B_MAIL_END_FETCH;
			
			BString command(uid), folder(uid), id;
			folder.Truncate(command.FindLast('/'));
			command.CopyInto(id,command.FindLast('/') + 1,command.Length());
			
			*out_folder_location = folder.String();
				
			Select(folder.String());
				
			if (out_headers->FindBool("ENTIRE_MESSAGE")) {				
				command = "UID FETCH ";
				command << id << " (FLAGS RFC822)";
				
				SendCommand(command.String());
				static char cmd[255];
				::sprintf(cmd,"a%.7ld"CRLF,commandCount);
				NestedString response;
				if (GetResponse(command,&response,true) != NOT_COMMAND_RESPONSE && command == cmd)
					return B_ERROR;
				
				for (int32 i = 0; i < response[2][1].CountItems(); i++) {
					//puts(response[2][1][i]());
					if (strcmp(response[2][1][i](),"\\Seen") == 0) {
						out_headers->AddString("STATUS","Read");
					}
				}
				WasCommandOkay(command);
				(*out_file)->WriteAt(0,response[2][5](),strlen(response[2][5]()));
				runner->ReportProgress(0,1);
				return B_OK;
			} else {
				command = "UID FETCH ";
				command << id << " (RFC822.SIZE FLAGS)";
				SendCommand(command.String());
				static char cmd[255];
				::sprintf(cmd,"a%.7ld"CRLF,commandCount);
				NestedString response;
				if (GetResponse(command,&response) != NOT_COMMAND_RESPONSE && command == cmd)
					return B_ERROR;
				WasCommandOkay(command);
				out_headers->AddInt32("SIZE",atoi(response[2][3]()));

				for (int32 i = 0; i < response[2][3].CountItems(); i++) {
					if (strcmp(response[2][3][i](),"\\Seen") == 0) {
						out_headers->AddString("STATUS","Read");
					}
				}
				*out_file = new IMAP4PartialReader(this,*out_file,id.String());
				return B_OK;
			}
		}

status_t IMAP4Client::DeleteMessage(const char* uid) {
	BString command(uid), folder(uid), id;
	folder.Truncate(command.FindLast('/'));
	command.CopyInto(id,command.FindLast('/') + 1,command.Length());
				
	command = "UID STORE ";
	command << id << " +FLAGS.SILENT (\\Deleted)";
	
	Select(folder.String());
	
	SendCommand(command.String());
	if (!WasCommandOkay(command)) {
		command.Prepend("Error while deleting message: ");
		runner->ShowError(command.String());
		return B_ERROR;
	}
	
	return B_OK;
}
	
status_t
IMAP4Client::SendCommand(const char* command)
{
	static char cmd[255];
	::sprintf(cmd,"a%.7ld %s"CRLF,++commandCount,command);
	net->Send(cmd,strlen(cmd));
	
	PRINT(("C: %s",cmd));
	
	return B_OK;
}

int32
IMAP4Client::ReceiveLine(BString &out)
{
	uint8 c = 0;
	int32 len = 0,r;
	out = "";
	if(net->IsDataPending(kIMAP4ClientTimeout))
	{
		while(c != '\n' && c != xEOF)
		{
			r = net->Receive(&c,1);
			if(r <= 0)
				break;
				
			out += (char)c;
			len += r;
		}
	}else{
		// Log an error somewhere instead
		runner->ShowError("IMAP Timeout.");
		len = -1;		
	}
	PRINT(("S:%s\n",out.String()));
	return len;
}

int IMAP4Client::GetResponse(BString &tag, NestedString *parsed_response, bool report_literals, bool internal_flag) {
	uint8 c = 0;
	int32 r;
	int8 delimiters_passed = internal_flag ? 2 : 0;
	int answer = NOT_COMMAND_RESPONSE;
	BString out;
	bool in_quote = false;
	bool was_cr = false;
	if(net->IsDataPending(kIMAP4ClientTimeout))
	{
		while(c != '\n' && c != xEOF)
		{
			r = net->Receive(&c,1);
			if(r <= 0)
				break;
			
			//putchar(c);
			
			if ((isspace(c) || (internal_flag && c == ')')) && !in_quote) {
				if (delimiters_passed == 0) {
					tag = out;
					out = "";
					delimiters_passed ++;
					continue;
				}
				if (delimiters_passed == 1) {
					
					if (out == "NO")
						answer = NO;
					else if (out == "BAD")
						answer = BAD;
					else if (out == "OK")
						answer = OK;
					else if (parsed_response != NULL && out != "")
						*parsed_response += out;
					
					out = "";
					delimiters_passed ++;
					continue;
				}
				
				if (c == '\r') {
					was_cr = true;
					continue;
				}

				if (c == '\n' && was_cr) {
					if (out.Length() == 0)
						return answer;
					if (out[0] == '{' && out[out.Length() - 1] == '}') {
						int octets_to_read;
						out.Truncate(out.Length() - 1);
						octets_to_read = atoi(out.String() + 1);
						out = "";
						char *buffer = new char[octets_to_read+1];
						buffer[octets_to_read] = 0;
						int read_octets = 0;
						int nibble_size;
						while (read_octets < octets_to_read) {
							nibble_size = net->Receive(buffer + read_octets,octets_to_read - read_octets);
							read_octets += nibble_size;
							if (report_literals)
								runner->ReportProgress(nibble_size,0);
						}
						
						if (parsed_response != NULL)
							parsed_response->AdoptAndAdd(buffer);
						else
							delete [] buffer;
							
						c = ' ';
						continue;
					}
				}
				
				if (internal_flag && (c == ')')) {
					if (parsed_response != NULL && out != "")
						(*parsed_response) += out;
					return answer;
				}
				
				was_cr = false;
				if (parsed_response != NULL && out != "")
					(*parsed_response) += out;
				out = "";
				continue;
			}
			
			was_cr = false;
			
			if (c == '\"') {
				in_quote = !in_quote;
				continue;
			}
			if (c == '(') {
				if (parsed_response != NULL)
					(*parsed_response) += NULL;
					
				BString trash;
				GetResponse(trash,&((*parsed_response)[parsed_response->CountItems() - 1]),report_literals,true);
				continue;
			}
			
			out += (char)c;
		}
	}else{
		// Log an error somewhere instead
		runner->ShowError("IMAP Timeout.");
	}
	return answer;
}

bool IMAP4Client::WasCommandOkay(BString &response) {
	do {
		response = "";
		if (ReceiveLine(response) < B_OK)
			runner->ShowError("No response from server");
	} while (response[0] == '*');
		
	static char cmd[255];
	::sprintf(cmd,"a%.7ld OK",commandCount);
	if (strncmp(response.String(),cmd,strlen(cmd)) == 0)
		return true;
	
	return false;
}

Mail::Filter *instantiate_mailfilter(BMessage *settings, Mail::ChainRunner *runner)
{
	return new IMAP4Client(settings,runner);
}
