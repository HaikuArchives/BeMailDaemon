/* IMAP4Client - implements the IMAP mail protocol
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "imap_client.h"

#include <stdlib.h>
#include <Alert.h>
#include <Debug.h>
#include <List.h>
#include <Path.h>

#include <StringList.h>
#include <status.h>
#include <crypt.h>

#include "imap_reader.h"

#include <MDRLanguage.h>

using namespace Zoidberg;


#define CRLF "\r\n"

#define xEOF    236

const bigtime_t kIMAP4ClientTimeout = 1000000*60; // 60 sec


/***********************************************************
 * Constructor.
 ***********************************************************/
IMAP4Client::IMAP4Client(BMessage *settings,Mail::StatusView *status)
	:BNetEndpoint(SOCK_STREAM)
	,Mail::Protocol(settings)
	,fCommandCount(0)
	,fIdleTime(0)
	,_settings(settings)
	,_status(status)
	,to_fetch(NULL)
{
//	BNetDebug::Enable(true);
	SetTimeout(kIMAP4ClientTimeout);
	error = Open(_settings->FindString("server"),settings->FindInt32("port"),settings->FindInt32("flavor"));

	if( B_OK == error )
	{
		const char *password = settings->FindString("password");
		char *passwd = get_passwd(settings,"cpasswd");
		if (passwd)
			password = passwd;
	
		BString uname = "\"";
		uname << settings->FindString("username") << "\"";
		error = Login(uname.String(), password, settings->FindInt32("auth_method"));
	}

	if( B_OK == error )
	{
		num_messages = Select(settings->FindString("folder"));
		_status->SetMessage(MDR_DIALECT_CHOICE ("Logged in","ログイン完了"));
	}
	else
	{
		BString errdesc(MDR_DIALECT_CHOICE ("Error Logging in: ","ログインエラー："));
		errdesc << strerror(error);
		PRINT((errdesc.String()));
		_status->SetMessage(errdesc.String());
	}
}


Mail::Filter* instantiate_mailfilter(BMessage *settings,Mail::StatusView *status) {
	return new IMAP4Client(settings,status);
}

status_t IMAP4Client::InitCheck(BString* out_message) {
	if (out_message != NULL)
		*out_message = strerror(error);
		
	return error;
}
/***********************************************************
 * Destructor
 ***********************************************************/
IMAP4Client::~IMAP4Client()
{
	// Deleting a NULL pointer is legal.
	delete to_fetch;

	Logout();
}

/***********************************************************
 * Connect
 ***********************************************************/
status_t
IMAP4Client::Open(const char* addr,int port,int)
{
	if (port <= 0)
		port = 143;
	
	_status->SetMessage(MDR_DIALECT_CHOICE ("Opening connection...","接続中..."));
	
	printf("Connecting to %s:%d\n", addr, port);
	
	_inherited::Connect(addr,port);
	
	printf("Connected!\n");
	
	BString out("");
	if( ReceiveLine(out) <= 0 )
		return B_ERROR;
	printf("Connected2!\n");
	// for re-connect
	fAddress = addr; fPort = port;
	//
	return B_OK;
}

/***********************************************************
 * Reconnect
 ***********************************************************/
status_t
IMAP4Client::Reconnect()
{
	fCommandCount = 0;
	status_t err;
	err = Open(fAddress.String(),fPort,-1);
	if(err != B_OK)
		return B_ERROR;
	err = Login(fLogin.String(),fPassword.String(),-1);
	if(err != B_OK)
		return B_ERROR;
	err = Select(fFolderName.String());
	if(err != B_OK)
		return B_ERROR;
	return B_OK;
}

/***********************************************************
 * Login
 ***********************************************************/
status_t
IMAP4Client::Login(const char* login,const char* password,int)
{
	status_t result = B_ERROR;
	_status->SetMessage(MDR_DIALECT_CHOICE ("Authenticating...","認証中..."));
	// for re-connect
	fLogin = login; fPassword = password;
	//
	BString cmd("LOGIN "),out("");
	cmd += login;
	cmd += " ";
	cmd += password;

	result = SendCommand(cmd.String());

	if( result != B_OK)
	{
		PRINT(("SendCommand() returns not B_OK\n"));
	}
	
	int32 cmdNumber = fCommandCount;
	int32 state,r;
	while( B_OK == result )
	{
		r = ReceiveLine(out);
		if(r <= 0)
		{
			PRINT(("ReceiveLine returns <= 0\n"));
			result = B_ERROR;
			break;
		}
		
		state = CheckSessionEnd(out.String(),cmdNumber);		
		switch(state)
		{
			case IMAP_SESSION_OK:
				result = B_OK;
				// lame, but this logic is whack;
				goto finish;

			case IMAP_SESSION_BAD:
				printf("CheckSessionEnd returns IMAP_SESSION_BAD\n");
				PRINT(("S: %s", out.String()));
				result = B_ERROR;
				break;
	
			case IMAP_SESSION_CONTINUED:
				// keep going
				break;
						
			default:
				printf("CheckSessionEnd returns %d, bad programmers!\n", (int)state);
				result = B_ERROR; 
				break;
		}
	}
	
finish:	
	return result;
}

/***********************************************************
 * List
 ***********************************************************/
status_t
IMAP4Client::UniqueIDs()
{
	if( num_messages < 0 )
	{
		return B_ERROR;
	}
	
	const char *folder_name = _settings->FindString("folder");
	BString cmd("FETCH ");
	
	cmd << "1:" << num_messages << " UID";
	
	BString out;
	SendCommand(cmd.String());
//	printf("Our command is: %s\n",cmd.String());
	int32 cmdNumber = fCommandCount;
	int32 state;
	int32 pos,i;
	int32 message_count = 0;
	char buf[B_FILE_NAME_LENGTH];
	BString temp;
	
	while(1)
	{
		if( 0 > ReceiveLine(out))
		{
			// There was an error!
			return B_ERROR;
		}
		
		state = CheckSessionEnd(out.String(),cmdNumber);

		switch(state)
		{
			case IMAP_SESSION_OK:
				return message_count;

			case IMAP_SESSION_BAD:
				return B_ERROR;
		}
		
		pos = out.FindFirst("(UID ") + 5;
		
		if( B_ERROR == pos )
		{
			// Problem with input.
			return B_ERROR;
		}
		
		i = out.FindFirst(')',pos);
		if( B_ERROR == i )
		{
			// another parsing problem
			return B_ERROR;
		}
		
		temp = "";
		out.CopyInto(temp,pos,i-pos);
		unique_ids->AddItem(temp.String());
	}
		
	return B_ERROR;
}

void IMAP4Client::PrepareStatusWindow(StringList *manifest) {
	to_fetch = new StringList;
	manifest->NotHere(*unique_ids,to_fetch);
	
	_status->SetTotalItems(to_fetch->CountItems());
}

/***********************************************************
 * Select
 ***********************************************************/
int32
IMAP4Client::Select(const char* folder_name)
{
	int32 r = 0;
	int32 mail_count = -1;
	
	BString cmd("SELECT ");
	cmd << "\"" << folder_name << "\"";
	
	BString out;
	int32 state;
	if( SendCommand(cmd.String()) == B_OK)
	{
		int32 cmdNumber = fCommandCount;
		while(1)
		{
			out = "";
			r = ReceiveLine(out);
			if(r <=0)
				break;
			// get mail count
			if((mail_count <0) && (out.IFindFirst("EXISTS") != -1))
				mail_count = atol(&(out.String()[2]));
				
			// check session end
			state = CheckSessionEnd(out.String(),cmdNumber);		
			switch(state)
			{
			case IMAP_SESSION_OK:
				PRINT(("Mail count:%d\n",mail_count));
				return mail_count;
			case IMAP_SESSION_BAD:
				return -1;
			}
		}
	}
	// for re-connect
	fFolderName = folder_name;
	//
	return mail_count;
}

/***********************************************************
 * Store
 ***********************************************************/
status_t
IMAP4Client::Store(int32 index,const char* flags,bool add)
{
	BString cmd("STORE ");
	cmd << index << " ";
	
	if(add)
		cmd += "+FLAGS ";
	else
		cmd += "-FLAGS ";
	cmd << "(" << flags << ")";
	BString out;
	int32 state;
	
	if(SendCommand(cmd.String()) == B_OK)
	{
		int32 cmdNumber = fCommandCount;
		
		while(1)
		{
			if( 0 > ReceiveLine(out))
			{
				// There was an error!
				return B_ERROR;
			}

			state = CheckSessionEnd(out.String(),cmdNumber);		
			switch(state)
			{
			case IMAP_SESSION_OK:
				return B_OK;
			case IMAP_SESSION_BAD:
				return B_ERROR;
			}
		}
	}
	return B_ERROR;	
}

/***********************************************************
 * MarkAsRead
 ***********************************************************/
status_t
IMAP4Client::MarkAsRead(int32 index)
{
	// Check connection
	if(!IsAlive())
	{
		PRINT(("Re-connect\n"));
		status_t err = Reconnect();
		if(err != B_OK)
			return B_ERROR;
	}
	//
	status_t err = Store(index,"\\Seen");
	return err;
}

/***********************************************************
 * MarkAsDelete
 ***********************************************************/
status_t
IMAP4Client::MarkAsDelete(int32 index)
{
	// Check connection
	if(!IsAlive())
	{
		PRINT(("Re-connect\n"));
		status_t err = Reconnect();
		if(err != B_OK)
			return B_ERROR;
	}
	//
	return Store(index,"\\Deleted");	
}

status_t IMAP4Client::DeleteMessage(const char* uid) {
	return MarkAsDelete(unique_ids->IndexOf(uid));
}
/***********************************************************
 * FetchField
 ***********************************************************/
/*status_t
IMAP4Client::FetchFields(int32 index,
					BString &subject,
					BString &from,
					BString &to,
					BString &cc,
					BString &reply,
					BString &date,
					BString &priority,
					bool	&read,
					bool	&attachment)
{
	// Check connection
	if(!IsAlive())
	{
		PRINT(("Re-connect\n"));
		status_t err = Reconnect();
		if(err != B_OK)
			return B_ERROR;
	}
	//
	BString cmd("FETCH ");
	cmd << index << " (FLAGS BODY[HEADER.FIELDS (Subject From To Reply-To Date X-Priority Content-Type)])";
	subject = from = to = date = cc = reply = "";
	attachment = read = false;
	priority = "3 (Normal)";	
	BString line;

	int32 r,state;
	int32 session = fCommandCount+1;

	char first_line[50];
	::sprintf(first_line,"* %ld FETCH",index);

	if( SendCommand(cmd.String()) == B_OK)
	{
		while(1)
		{
			r = ReceiveLine(line);
			if(r <=0)
				break;
			//PRINT(("%s\n",line.String() ));
			line.ReplaceAll("\r\n","");
			const char* p = line.String();
			// Check flags
			if(strncmp(first_line,p,strlen(first_line)) == 0)
			{
				read = (line.FindFirst("\\Seen") != B_ERROR)?true:false;
			}
			// Subject
			if(strncmp("Subject:",p,8) == 0)
				subject = &p[9];
			// Date
			else if(strncmp("Date:",p,5) == 0)
				date = &p[6];
			// From
			else if(strncmp("From:",p,5) == 0)
				from = &p[6];
			// To
			else if(strncmp("To:",p,3) == 0)
				to = &p[4];
			// Cc,
			else if(strncmp("Cc:",p,3) == 0)
				cc = &p[4];
			// Reply
			else if(strncmp("Reply-To:",p,9) == 0)
				reply = &p[10];
			// Priority
			else if(strncmp("X-Priority:",p,11) == 0)
				priority = &p[12];
			// Content-Type
			else if(strncmp("Content-Type:",p,13) == 0)
			{
				p += 14;
				attachment = (strncmp(p,"multipart",9) == 0)?true:false;	
			}
			state = CheckSessionEnd(line.String(),session);		
			switch(state)
			{
			case IMAP_SESSION_OK:
				return B_OK;
			case IMAP_SESSION_BAD:
				return B_ERROR;
			}
		}
	}
	return B_ERROR;
}*/

/***********************************************************
 * Noop
 ***********************************************************/
status_t
IMAP4Client::Noop()
{
	PRINT(("Send Noop\n"));
	BString out;
	int32 index = fCommandCount+1;
	
	if( SendCommand("NOOP") != B_OK)
		return B_ERROR;
	char end_line[50];
	::sprintf(end_line,"%.3ld OK NOOP",index);
	
	while(1)
	{
		if( 0 > ReceiveLine(out))
		{
			// There was an error!
			return B_ERROR;
		}

		if(::strncmp(out.String(),end_line,strlen(end_line))==0)
			break;
	}
	
	if( out[4] == 'O' && out[5] == 'K')
		return B_OK;
	return B_ERROR;
}

/***********************************************************
 * IsAlive
 ***********************************************************/
bool
IMAP4Client::IsAlive()
{
	time_t now = time(NULL);
	// If idle time is more than 10 min
	// Check connection with NOOP command
	if(difftime(now,fIdleTime) <= 600)
		return true;
	return (Noop() == B_OK)?true:false;
}

/***********************************************************
 * Logout
 ***********************************************************/
void
IMAP4Client::Logout()
{
	BString out;

	if( B_OK == error )
	{
		SendCommand("CLOSE");
		ReceiveLine(out);
	}

	SendCommand("LOGOUT");
	ReceiveLine(out);
	
	Close();
}

/***********************************************************
 * SendCommand
 ***********************************************************/
status_t
IMAP4Client::SendCommand(const char* command)
{
	printf("[%s:%d] C: '%s'\n", fAddress.String(), (int)fPort, command);
	BString out("");
	status_t err = B_ERROR;
	char *cmd = new char[strlen(command) + 15];
	if(!cmd)
	{
		(new BAlert("","Memory was exhausted","OK",NULL,NULL,B_WIDTH_AS_USUAL,B_STOP_ALERT))->Go();
		return B_ERROR;
	}

	// If command number field is over 7 field,
	// reset command number.
	if(fCommandCount > 9999999)
		fCommandCount = 0;
// \r\n
	::sprintf(cmd,"%.7ld %s" CRLF,++fCommandCount,command);
	PRINT(("FULL C: '%s'\n", cmd));
	int32 cmd_len = strlen(cmd);
	if(!strstr(cmd,"LOGIN"))
		PRINT(("C:%s",cmd));
	// Reset idle time
	fIdleTime = time(NULL);
	//
	if( Send(cmd,cmd_len) == cmd_len)
		err = B_OK;
	delete[] cmd;
	return err;
}

/***********************************************************
 * ReceivePendingData
 ***********************************************************/
int32
IMAP4Client::ReceiveLine(BString &out)
{
	unsigned int c = 0;
	int32 len = 0,r;
	out = "";
	if(IsDataPending(kIMAP4ClientTimeout))
	{
		while(c != '\n' && c != EOF && c != xEOF)
		{
			r = Receive(&c,1);
			if(r <= 0)
				break;
				
			//putchar(c);
			out += (char)c;
			len += r;
		}
	}else{
		// (new BAlert("","IMAP4 socket timeout.","OK"))->Go();
		// Log an error somewhere instead
		_status->SetMessage("IMAP Timeout.");
		len = -1;		
	}
	PRINT(("S:%s\n",out.String()));
	return len;
}

/***********************************************************
 * CheckSessionEnd
 ***********************************************************/
int32
IMAP4Client::CheckSessionEnd(const char* str,int32 session)
{
	int32 result = IMAP_SESSION_CONTINUED;
	char session_end[9];

	::sprintf(session_end,"%.7ld ",session);
	
	if( ::strncmp(session_end,str,8) == 0)
	{
		if( str[8] == 'O' && str[9] == 'K')
			result = IMAP_SESSION_OK;
		else if( str[8] == 'B' && str[9] == 'A' && str[10] == 'D')
			result = IMAP_SESSION_BAD;
		else if( str[8] == 'N' && str[9] == 'O')
			result = IMAP_SESSION_BAD;
	}
	
	return result;
}

status_t IMAP4Client::GetNextNewUid
	(
		BString* out_uid,
		StringList *manifest,
		time_t timeout
	) {
		static int last_viewed = 0;
		
		if (last_viewed >= to_fetch->CountItems())
			return B_NAME_NOT_FOUND;
		
		out_uid->SetTo(to_fetch->ItemAt(last_viewed));
		last_viewed++;
		
		return B_OK;
	}
		
status_t IMAP4Client::GetMessage(
	const char* uid,
	BPositionIO** out_file, BMessage* out_headers,
	BPath* out_folder_location
) {
	int32 seq_id = unique_ids->IndexOf(uid);
	if (seq_id < 0)
		return B_NAME_NOT_FOUND;

	_status->SetMessage((BString(MDR_DIALECT_CHOICE ("Retrieving message id","メッセージの検索中")) << seq_id).String());

	*out_file = new IMAP4Reader(this,*out_file,uid);
	out_folder_location->SetTo(_settings->FindString("folder"));
	
	return B_OK;
}
