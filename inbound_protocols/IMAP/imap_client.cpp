#include <MailProtocol.h>
#include <NetEndpoint.h>
#include <Message.h>
#include <ChainRunner.h>
#include <MDRLanguage.h>
#include <Debug.h>
#include <StringList.h>
#include <MessageRunner.h>
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

enum { OK,BAD,NO,NOT_COMMAND_RESPONSE };

class IMAP4Client : public Mail::Protocol {
	public:
		IMAP4Client(BMessage *settings, Mail::ChainRunner *run);
		//virtual ~IMAP4Client();
		
		virtual status_t GetMessage(
			const char* uid,
			BPositionIO** out_file, BMessage* out_headers,
			BPath* out_folder_location);
			
		virtual status_t DeleteMessage(const char* uid) { return B_ERROR; }
		
		status_t ReceiveLine(BString &out);
		status_t SendCommand(const char *command);
		
		virtual status_t InitCheck(BString *) { return B_OK; }
		
		int GetResponse(BString &tag, NestedString *parsed_response, bool report_fetch_results = false, bool recursion_flag = false, bool b = false);
		bool WasCommandOkay(BString &response);
		
		void InitializeMailboxes();
		void SyncAllBoxes();
	
	private:
		int32 commandCount;
		BNetEndpoint *net;
		BString selected_mb;
		StringList boxes;
		status_t err;
};

class SyncHandler : public BHandler {
	public:
		SyncHandler(IMAP4Client *us) : BHandler(), client(us) {}
		~SyncHandler() { delete runner; }
		void MessageReceived(BMessage *msg) {
			if (msg->what != 'imps' /*IMaP Sync */)
				return;
				
			client->SyncAllBoxes(); //--- Not the most lightweight solution, but simple.
		}
	
		BMessageRunner *runner;
		
	private:
		IMAP4Client *client;
};

IMAP4Client::IMAP4Client(BMessage *settings, Mail::ChainRunner *run) : Mail::Protocol(settings,run), commandCount(1), net(NULL), selected_mb("") {
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
	
	InitializeMailboxes();
	SyncAllBoxes();
	
	SyncHandler *sync = new SyncHandler(this);
	runner->AddHandler(sync);
	sync->runner = new BMessageRunner(BMessenger(sync,runner),new BMessage('imps'),30e6);
}

void IMAP4Client::InitializeMailboxes() {
	SendCommand("LIST \"\" \"*\"");
	BString tag;
	char expected[255];
	::sprintf(expected,"a%.7ld",commandCount);
	int val;
	do {
		NestedString response;
		val = GetResponse(tag,&response);
		if (val != NOT_COMMAND_RESPONSE)
			break;
				
		if (tag == expected)
			break;
		
		if (response[3]()[0] != '.')
			boxes += response[3]();
	} while (1);
	
	/*puts("Mailboxes:");
	for (int32 i = 0; i < boxes.CountItems(); i++)
		printf("\t%s\n",boxes[i]);*/
}

#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");

void IMAP4Client::SyncAllBoxes() {
	BString command;
	char expected[255];
	StringList *list = new StringList;
	BString uid,tag;
	if (selected_mb != "") {
		SendCommand("CLOSE");
		WasCommandOkay(command);
	}
	for (int32 i = 0; i < boxes.CountItems(); i++) {
		int32 num_messages = -1;
		
		command = "SELECT \"";
		command << boxes[i] << '\"';
		
		SendCommand(command.String());
		::sprintf(expected,"a%.7ld",commandCount);
		while(1) {
			NestedString response;
			GetResponse(tag,&response);
			
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
	}
	
	delete unique_ids;
	unique_ids = list;
	
	StringList to_dl;
	unique_ids->NotThere(*manifest,&to_dl);
	
	runner->GetMessages(&to_dl,102400 /* I don't want to look up sizes just now */);
}

status_t IMAP4Client::GetMessage(
	const char* uid, BPositionIO** out_file, BMessage* out_headers,
	BPath* out_folder_location) {
			if (strcmp("//!newmsgcheck",uid) == 0) {
				SyncAllBoxes();
				return B_MAIL_END_FETCH;
			}
			
			BString command(uid), folder(uid), id;
			folder.Truncate(command.FindLast('/'));
			command.CopyInto(id,command.FindLast('/') + 1,command.Length());
						
			command = "UID FETCH ";
			command << id << " (FLAGS RFC822)";
			
			if (selected_mb != folder) {
				if (selected_mb != "") {
					BString trash;
					SendCommand("CLOSE");
					WasCommandOkay(trash);
				}
				BString cmd = "SELECT ";
				cmd << folder;
				SendCommand(cmd.String());
				if (!WasCommandOkay(cmd)) {
					runner->ShowError(cmd.String());
					return B_ERROR;
				}
				
				selected_mb = folder;
			}
			
			SendCommand(command.String());
			static char cmd[255];
			::sprintf(cmd,"a%.7ld %s"CRLF,++commandCount,command.String());
			NestedString response;
			if (GetResponse(command,&response,true) != NOT_COMMAND_RESPONSE)
				return B_ERROR;
				
			//response.PrintToStream();
			WasCommandOkay(command);
			(*out_file)->Write(response[2][5](),strlen(response[2][5]()));
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

int IMAP4Client::GetResponse(BString &tag, NestedString *parsed_response, bool report_fetch_results, bool internal_flag, bool always_report) {
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
						if ((!report_fetch_results || strcmp(parsed_response[0](), "FETCH")) && !always_report)
							net->Receive(buffer,octets_to_read);
						else {
							int read_bytes;
							while (octets_to_read > 0) {
								read_bytes = net->Receive(buffer,(octets_to_read > 255) ? 255 : octets_to_read);
								/*if (read_bytes < 0)
									return read_bytes;*/
								
								octets_to_read -= read_bytes;
								runner->ReportProgress(read_bytes,0);
							}
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
				GetResponse(trash,&((*parsed_response)[parsed_response->CountItems() - 1]),report_fetch_results,true,report_fetch_results && !strcmp(parsed_response[0](), "FETCH"));
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
