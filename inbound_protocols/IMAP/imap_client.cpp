#include <RemoteStorageProtocol.h>
#include <socket.h>
#include <netdb.h>
#include <errno.h>
#include <Message.h>
#include <ChainRunner.h>
#include <MDRLanguage.h>
#include <Debug.h>
#include <StringList.h>
#include <Entry.h>
#include <Directory.h>
#include <File.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
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

enum { OK,BAD,NO,CONTINUE, NOT_COMMAND_RESPONSE };

struct mailbox_info {
	int32 exists;
	int32 next_uid;
	BString server_mb_name;
};

class IMAP4Client : public Mail::RemoteStorageProtocol {
	public:
		IMAP4Client(BMessage *settings, Mail::ChainRunner *run);
		virtual ~IMAP4Client();
		
		virtual status_t GetMessage(const char *mailbox, const char *message, BPositionIO **, BMessage *headers);
		virtual status_t AddMessage(const char *mailbox, BPositionIO *data, BString *id);
		
		virtual status_t DeleteMessage(const char *mailbox, const char *message);
		virtual status_t CopyMessage(const char *mailbox, const char *to_mailbox, BString *message);
		
		virtual status_t CreateMailbox(const char *mailbox);
		virtual status_t DeleteMailbox(const char *mailbox);
		
		void GetUniqueIDs();
		
		status_t ReceiveLine(BString &out);
		status_t SendCommand(const char *command);
		
		status_t Select(const char *mb, bool force_reselect = false);
		status_t Close();
		
		virtual status_t InitCheck(BString *) { return err; }
		
		int GetResponse(BString &tag, NestedString *parsed_response, bool report_literals = false, bool recursion_flag = false);
		bool WasCommandOkay(BString &response);
		
		void InitializeMailboxes();
	
	private:
		friend class NoopWorker;
		friend class IMAP4PartialReader;
		
		NoopWorker *noop;
		BMessageRunner *nooprunner;
		
		int32 commandCount;
		int net;
		BString selected_mb, inbox_name, hierarchy_delimiter, mb_root;
		int32 inbox_index;
		BList box_info;
		status_t err;
};

class NoopWorker : public BHandler {
	public:
		NoopWorker(IMAP4Client *a) : us(a) {}
		void RecentMessages() {
			int32 num_messages = -1;
			int32 next_uid = -1;
			int32 total = 0;
			char expected[255];
			BString tag;
			::sprintf(expected,"a%.7ld",us->commandCount);
			while(1) {
				NestedString response;
				if (us->GetResponse(tag,&response) < 0)
					return;
				
				if (tag == expected)
					break;
				
				if (response[0].CountItems() == 2 && strcasecmp(response[0][0](),"UIDNEXT") == 0)
					next_uid = atol(response[0][1]());
					
				if (response.CountItems() < 2)
					continue;
				
				if (strcasecmp(response[1](),"RECENT") == 0)
					num_messages = atoi(response[0]());
				if (strcasecmp(response[1](),"EXISTS") == 0)
					total = atoi(response[0]());
			}
			
			
			if ((num_messages < 0) || (total < 0))
				return;
				
			struct mailbox_info *stats = (struct mailbox_info *)us->box_info.ItemAt(us->inbox_index);
			stats->exists = total;
			stats->next_uid = next_uid;
			
			if ((num_messages == 0) || (total == 0))
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
				if (us->GetResponse(tag,&response) < 0)
					return;
							
				if (tag == expected)
					break;
				
				uid = us->inbox_name;
				uid << '/' << response[2][1]();
				if (!us->unique_ids->HasItem(uid.String()))
					list.AddItem(uid.String());
			}
			
			(*us->unique_ids) += list;
			us->runner->GetMessages(&list,-1);
		}
		void MessageReceived(BMessage *msg) {
			if (msg->what != 'impn' /* IMaP Noop */)
				return;
				
			if (strcasecmp(us->selected_mb.String(),"INBOX"))
				us->Close();
			
			if (us->selected_mb == "") {
				BString command = "SELECT ";
				command << us->inbox_name;
				us->SendCommand(command.String());
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

IMAP4Client::IMAP4Client(BMessage *settings, Mail::ChainRunner *run) : Mail::RemoteStorageProtocol(settings,run), commandCount(0), net(-1), selected_mb(""), inbox_index(-1), noop(NULL) {
	err = B_OK;
	
	mb_root = settings->FindString("root");
	
	int port = settings->FindInt32("port");
	if (port <= 0)
		port = 143;

//-----Open TCP link	
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Opening connection...","接続中..."));
	
	uint32 hostIP = inet_addr(settings->FindString("server"));  // first see if we can parse it as a numeric address
	if ((hostIP == 0)||(hostIP == (uint32)-1)) {
		struct hostent * he = gethostbyname(settings->FindString("server"));
		hostIP = he ? *((uint32*)he->h_addr) : 0;
	}
   
	if (hostIP == 0) {
		BString error;
		error << "Could not connect to IMAP server " << settings->FindString("server");
		if (port != 143)
			error << ":" << port;
		error << ": Host not found.";
		runner->ShowError(error.String());
		runner->Stop();
		return;
	}
	
	net = socket(AF_INET, SOCK_STREAM, 0);
	if (net >= 0) {
		struct sockaddr_in saAddr;
		memset(&saAddr, 0, sizeof(saAddr));
		saAddr.sin_family      = AF_INET;
		saAddr.sin_port        = htons(port);
		saAddr.sin_addr.s_addr = hostIP;
		int result = connect(net, (struct sockaddr *) &saAddr, sizeof(saAddr));
		if (result < 0) {
			closesocket(net);
			net = -1;
			BString error;
			error << "Could not connect to IMAP server " << settings->FindString("server");
			if (port != 143)
				error << ":" << port;
			error << '.';
			runner->ShowError(error.String());
			runner->Stop();
			return;
		}
	} else {
		BString error;
		error << "Could not connect to IMAP server " << settings->FindString("server");
		if (port != 143)
			error << ":" << port;
		error << ". (" << strerror(errno) << ')';
		runner->ShowError(error.String());
		runner->Stop();
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
		response.Prepend("Login failed. Please check your username and password.\n(");
		response << ')';
		runner->ShowError(response.String());
		err = B_ERROR;
		runner->Stop();
		return;
	}
	
	runner->ReportProgress(0,0,"Logged in");
	
	InitializeMailboxes();
	GetUniqueIDs();
	
	StringList to_dl;
	unique_ids->NotThere(*manifest,&to_dl);
	
	if (to_dl.CountItems() > 0)
		runner->GetMessages(&to_dl,-1);
	
	noop = new NoopWorker(this);
	runner->AddHandler(noop);
	nooprunner = new BMessageRunner(BMessenger(noop,runner),new BMessage('impn'),10e6);
}

IMAP4Client::~IMAP4Client() {	
	if (selected_mb != "")
		SendCommand("CLOSE");
	SendCommand("LOGOUT");
	
	for (int32 i = 0; i < box_info.CountItems(); i++)
		delete (struct mailbox_info *)(box_info.ItemAt(i));
	
	delete noop;
	closesocket(net);
}

void IMAP4Client::InitializeMailboxes() {
	BString command;
	command << "LIST \"" << mb_root << "\" \"*\"";
	
	SendCommand(command.String());
	
	BString tag;
	char expected[255];
	::sprintf(expected,"a%.7ld",commandCount);
	create_directory(runner->Chain()->MetaData()->FindString("path"),0777);
	const char *path = runner->Chain()->MetaData()->FindString("path");
	int val;
	do {
		NestedString response;
		val = GetResponse(tag,&response);
		if (val != NOT_COMMAND_RESPONSE)
			break;
				
		if (tag == expected)
			break;
		
		if (response[3]()[0] != '.') {
			struct mailbox_info *info = new struct mailbox_info;
			info->exists = -1;
			info->next_uid = -1;
			info->server_mb_name = response[3]();
			box_info.AddItem(info);
			BString parsed_name = response[3]();
			if ((mb_root != "") &&  (strncmp(mb_root.String(),parsed_name.String(),mb_root.Length()) == 0))
				parsed_name.Remove(0,mb_root.Length());
			
			if (strcasecmp(response[2](),"NIL")) {
				hierarchy_delimiter = response[2]();
				if (strcmp(response[2](),"/")) {
					if (strcmp(response[2](),"\\"))
						parsed_name.ReplaceAll('/','\\');
					else
						parsed_name.ReplaceAll('/','-');
						
					parsed_name.ReplaceAll(response[2](),"/");
				}
			}
			if (parsed_name.ByteAt(0) == '/')
				parsed_name.Remove(0,1);
				
			mailboxes += parsed_name.String();
			if (strcasecmp(parsed_name.String(),"INBOX") == 0) {
				inbox_name = response[3]();
				inbox_index = box_info.CountItems() - 1;
			}
				
			BPath blorp(path);
			blorp.Append(parsed_name.String());
			create_directory(blorp.Path(),0777);
		}
	} while (1);
	
	
	if (hierarchy_delimiter == "" || hierarchy_delimiter == "NIL") {
		SendCommand("LIST \"\" \"\"");
		NestedString dem;
		GetResponse(tag,&dem);
		hierarchy_delimiter = dem[2]();
		if (hierarchy_delimiter == "" || hierarchy_delimiter == "NIL")
			hierarchy_delimiter = "/";
	}
	
	if (mb_root.ByteAt(mb_root.Length() - 1) != hierarchy_delimiter.ByteAt(0)) {	
		command = "SELECT ";
		command << mb_root;
		SendCommand(command.String());
		if (WasCommandOkay(command)) {
			struct mailbox_info *info = new struct mailbox_info;
			info->exists = -1;
			info->next_uid = -1;
			info->server_mb_name = mb_root;
			
			mailboxes += "";
			box_info.AddItem(info);
			
			if (strcasecmp(mb_root.String(),"INBOX") == 0) {
				inbox_name = mb_root;
				inbox_index = box_info.CountItems() - 1;
			}
		}
	}
}

#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");

status_t IMAP4Client::AddMessage(const char *mailbox, BPositionIO *data, BString *id) {
	Select(mailbox,true);
	Close();
	
	const int32 box_index = mailboxes.IndexOf(mailbox);
	char expected[255];
	BString tag;
	
	BString command = "APPEND \"";
	off_t size;
	data->Seek(0,SEEK_END);
	size = data->Position();
	command << ((struct mailbox_info *)(box_info.ItemAt(box_index)))->server_mb_name << "\" {" << size << '}';
	SendCommand(command.String());
	ReceiveLine(command);
	char *buffer = new char[size];
	data->ReadAt(0,buffer,size);
	send(net,buffer,size,0);
	send(net,"\r\n",2,0);
	WasCommandOkay(command);
	
	if (((struct mailbox_info *)(box_info.ItemAt(box_index)))->next_uid <= 0) {
		Select(mailbox,true);
		
		command = "FETCH ";
		command << ((struct mailbox_info *)(box_info.ItemAt(box_index)))->exists << " UID";
		
		SendCommand(command.String());
		::sprintf(expected,"a%.7ld",commandCount);
		*id = "";
		while (1) {
			NestedString response;
			GetResponse(tag,&response);
						
			if (tag == expected)
				break;
			
			*id = response[2][1]();
		}
	} else {
		*id = "";
		*id << ((struct mailbox_info *)(box_info.ItemAt(box_index)))->next_uid;
	}
	
	return B_OK;
}

status_t IMAP4Client::DeleteMessage(const char *mailbox, const char *message) {
	BString command = "UID STORE ";
	command << message << " +FLAGS.SILENT (\\Deleted)";
	
	if (Select(mailbox) < B_OK)
		return B_ERROR;
	
	SendCommand(command.String());
	if (!WasCommandOkay(command)) {
		command.Prepend("Error while deleting message: ");
		runner->ShowError(command.String());
		return B_ERROR;
	}
	
	return B_OK;
}

status_t IMAP4Client::CopyMessage(const char *mailbox, const char *to_mailbox, BString *message) {
	struct mailbox_info *to_mb = (struct mailbox_info *)(box_info.ItemAt(mailboxes.IndexOf(to_mailbox)));
	char expected[255];
	BString tag;
	
	Select(mailbox);
	
	BString command = "UID COPY ";
	command << *message << " \"" << to_mb->server_mb_name << '\"';
	SendCommand(command.String());
	if (!WasCommandOkay(command))
		return B_ERROR;
	
	Select(to_mailbox,true);
	
	if (to_mb->next_uid <= 0) {
		command = "FETCH ";
		command << to_mb->exists << " UID";
		
		SendCommand(command.String());
		::sprintf(expected,"a%.7ld",commandCount);
		*message = "";
		while (1) {
			NestedString response;
			GetResponse(tag,&response);
						
			if (tag == expected)
				break;
			
			*message = response[2][1]();
		}
	} else {
		*message = "";
		*message << (to_mb->next_uid - 1);
	}
	
	return B_OK;
}

status_t IMAP4Client::CreateMailbox(const char *mailbox) {
	Close();
	
	struct mailbox_info *info = new struct mailbox_info;
	info->exists = -1;
	info->next_uid = -1;
	info->server_mb_name = mailbox;
	info->server_mb_name.ReplaceAll("/",hierarchy_delimiter.String());
	if (mb_root.ByteAt(mb_root.Length() - 1) != hierarchy_delimiter.ByteAt(0))
		info->server_mb_name.Prepend(hierarchy_delimiter);
		
	info->server_mb_name.Prepend(mb_root.String());
	
	BString command;
	command << "CREATE \"" << info->server_mb_name << '\"';
	SendCommand(command.String());
	BString response;
	if (!WasCommandOkay(response)) {
		command = "Error creating mailbox ";
		command << mailbox << ". The server said: \n" << response << "\nThis may mean you can't create a new mailbox in this location.";
		runner->ShowError(command.String());
		delete info;
		return B_ERROR;
	}
	
	box_info.AddItem(info);
	
	return B_OK;
}

status_t IMAP4Client::DeleteMailbox(const char *mailbox) {
	Close();
	
	if (!mailboxes.HasItem(mailbox))
		return B_ERROR;
	
	BString command;
	command << "DELETE \"" << ((struct mailbox_info *)(box_info.ItemAt(mailboxes.IndexOf(mailbox))))->server_mb_name << '\"';
	
	box_info.RemoveItem(mailboxes.IndexOf(mailbox));
	
	SendCommand(command.String());
	if (!WasCommandOkay(command)) {
		command = "Error deleting mailbox ";
		command << mailbox << '.';
		runner->ShowError(command.String());
		
		return B_ERROR;
	}
	
	return B_OK;
}

void IMAP4Client::GetUniqueIDs() {
	BString command;
	char expected[255];
	BString tag;
	BString uid;
	struct mailbox_info *info;
	
	for (int32 i = 0; i < mailboxes.CountItems(); i++) {
		Select(mailboxes[i]);
		
		info = (struct mailbox_info *)(box_info.ItemAt(i));
		if (info->exists <= 0)
			continue;
		
		command = "FETCH 1:";
		command << info->exists << " UID";
		SendCommand(command.String()); 
		
		::sprintf(expected,"a%.7ld",commandCount);
		while(1) {
			NestedString response;
			GetResponse(tag,&response);
						
			if (tag == expected)
				break;
			
			uid = mailboxes[i];
			uid << '/' << response[2][1]();
			unique_ids->AddItem(uid.String());
		}
	}
}

status_t IMAP4Client::Close() {
	if (selected_mb != "") {
		BString worthless;
		SendCommand("CLOSE");
		if (!WasCommandOkay(worthless))
			return B_ERROR;
			
		selected_mb = "";
	}
	
	return B_OK;
}

status_t IMAP4Client::Select(const char *mb, bool reselect) {
	if (reselect)
		Close();
	
	struct mailbox_info *info = (struct mailbox_info *)(box_info.ItemAt(mailboxes.IndexOf(mb)));
	if (info == NULL)
		return B_NAME_NOT_FOUND;
		
	const char *real_mb = info->server_mb_name.String();
	
	if (selected_mb != real_mb) {
		if (selected_mb != "") {
			BString trash;
			SendCommand("CLOSE");
			selected_mb = "";
			WasCommandOkay(trash);
		}
		BString cmd = "SELECT \"";
		cmd << real_mb << '\"';
		SendCommand(cmd.String());
		
		char expected[255];
		BString tag;
		::sprintf(expected,"a%.7ld",commandCount);
		
		while(1) {
			NestedString response;
			GetResponse(tag,&response);
			
			if (tag == expected)
				break;
			
			if ((response.CountItems() > 1) && (strcasecmp(response[1](),"EXISTS") == 0))
				info->exists = atoi(response[0]());
			
			if (response[0].CountItems() == 2 && strcasecmp(response[0][0](),"UIDNEXT") == 0)
				info->next_uid = atol(response[0][1]());
		}
		
		selected_mb = real_mb;
	}
		
	return B_OK;
}

class IMAP4PartialReader : public BPositionIO {
	public:
		IMAP4PartialReader(IMAP4Client *client,BPositionIO *_slave,const char *id) : us(client), slave(_slave), done(false) {
			strcpy(unique,id);
		}
		~IMAP4PartialReader() {
			delete slave;
			us->runner->ReportProgress(0,1);
		}
		off_t Seek(off_t position, uint32 seek_mode) {			
			if (seek_mode == SEEK_END) {
				if (!done) {
					slave->Seek(0,SEEK_END);
					FetchMessage("RFC822.TEXT");
				}
				done = true;
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
			while ((bytes = slave->ReadAt(pos,buffer,amountToWrite)) < amountToWrite && !done) {
					slave->Seek(0,SEEK_END);
					FetchMessage("RFC822.TEXT");
					done = true;
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
			
			//response.PrintToStream();
			
			us->WasCommandOkay(command);
			for (int32 i = 0; (i+1) < response[2].CountItems(); i++) {
				if (strcmp(response[2][i](),part) == 0) {
					slave->Write(response[2][i+1](),strlen(response[2][i+1]()));
					break;
				}
			}
			
		}
			
		IMAP4Client *us;
		char unique[25];
		BPositionIO *slave;
		bool done;
};

status_t IMAP4Client::GetMessage(const char *mailbox, const char *message, BPositionIO **data, BMessage *headers) {	
	Select(mailbox);
	
	if (headers->FindBool("ENTIRE_MESSAGE")) {				
		BString command = "UID FETCH ";
		command << message << " (FLAGS RFC822)";
		
		SendCommand(command.String());
		static char cmd[255];
		::sprintf(cmd,"a%.7ld"CRLF,commandCount);
		NestedString response;
		if (GetResponse(command,&response,true) != NOT_COMMAND_RESPONSE && command == cmd)
			return B_ERROR;
		
		for (int32 i = 0; i < response[2][1].CountItems(); i++) {
			if (strcmp(response[2][1][i](),"\\Seen") == 0) {
				headers->AddString("STATUS","Read");
			}
		}
		
		WasCommandOkay(command);
		(*data)->WriteAt(0,response[2][5](),strlen(response[2][5]()));
		runner->ReportProgress(0,1);
		return B_OK;
	} else {
		BString command = "UID FETCH ";
		command << message << " (RFC822.SIZE FLAGS RFC822.HEADER)";
		SendCommand(command.String());
		static char cmd[255];
		::sprintf(cmd,"a%.7ld"CRLF,commandCount);
		NestedString response;
		if (GetResponse(command,&response) != NOT_COMMAND_RESPONSE && command == cmd)
			return B_ERROR;
		
		WasCommandOkay(command);
		headers->AddInt32("SIZE",atoi(response[2][3]()));

		for (int32 i = 0; i < response[2][5].CountItems(); i++) {
			if (strcmp(response[2][5][i](),"\\Seen") == 0)
				headers->AddString("STATUS","Read");
		}
		
		(*data)->Write(response[2][7](),strlen(response[2][7]()));
		
		*data = new IMAP4PartialReader(this,*data,message);
		return B_OK;
	}
}

status_t
IMAP4Client::SendCommand(const char* command)
{
	static char cmd[255];
	::sprintf(cmd,"a%.7ld %s"CRLF,++commandCount,command);
	send(net,cmd,strlen(cmd),0);
	
	PRINT(("C: %s",cmd));
	
	return B_OK;
}

int32
IMAP4Client::ReceiveLine(BString &out)
{
	uint8 c = 0;
	int32 len = 0,r;
	out = "";
	
	struct timeval tv;
	struct fd_set fds; 

	tv.tv_sec = long(kIMAP4ClientTimeout / 1e6); 
	tv.tv_usec = long(kIMAP4ClientTimeout-(tv.tv_sec * 1e6)); 
	
	/* Initialize (clear) the socket mask. */ 
	FD_ZERO(&fds);
	
	/* Set the socket in the mask. */ 
	FD_SET(net, &fds); 
	int result = select(32, &fds, NULL, NULL, &tv);
	
	if (result < 0)
		return errno;
	
	if(result > 0)
	{
		while(c != '\n' && c != xEOF)
		{
			r = recv(net,&c,1,0);
			if(r <= 0) {
				BString error;
				error << "Connection to " << settings->FindString("server") << " lost.";
				runner->Stop();
				runner->ShowError(error.String());
				return -1;
			}
				
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
	int result;
	
	{
		struct timeval tv;
		struct fd_set fds; 
	
		tv.tv_sec = long(kIMAP4ClientTimeout / 1e6); 
		tv.tv_usec = long(kIMAP4ClientTimeout-(tv.tv_sec * 1e6)); 
		
		/* Initialize (clear) the socket mask. */ 
		FD_ZERO(&fds);
		
		/* Set the socket in the mask. */ 
		FD_SET(net, &fds); 
		result = select(32, &fds, NULL, NULL, &tv);
	}
	
	if (result < 0)
		return errno;
		
	if(result > 0)
	{
		while(c != '\n' && c != xEOF)
		{
			r = recv(net,&c,1,0);
			if(r <= 0) {
				BString error;
				error << "Connection to " << settings->FindString("server") << " lost.";
				runner->Stop();
				runner->ShowError(error.String());
				return -1;
			}
			
			//putchar(c);
			
			if ((isspace(c) || (internal_flag && (c == ')' || c == ']'))) && !in_quote) {
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
							nibble_size = recv(net,buffer + read_octets,octets_to_read - read_octets,0);
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
				
				if (internal_flag && (c == ')' || c == ']')) {
					if (parsed_response != NULL && out != "")
						(*parsed_response) += out;
					break;
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
			if (c == '(' || c == '[') {
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
		if (ReceiveLine(response) < B_OK) {
			runner->ShowError("No response from server");
			return false;
		}
	} while (response[0] == '*');
		
	bool to_ret = false;
	static char cmd[255];
	::sprintf(cmd,"a%.7ld OK",commandCount);
	if (strncmp(response.String(),cmd,strlen(cmd)) == 0)
		to_ret = true;
	
	int32 i = response.FindFirst(' ');
	i = response.FindFirst(' ',i+1);
	response.Remove(0,i+1);
	response.ReplaceAll("\r\n","\n");
	for (int32 i = response.Length()-1; response.String()[i] == '\n'; i--)
		response.Truncate(i);
	
	return to_ret;
}

Mail::Filter *instantiate_mailfilter(BMessage *settings, Mail::ChainRunner *runner)
{
	return new IMAP4Client(settings,runner);
}
