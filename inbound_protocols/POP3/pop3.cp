/* POP3Protocol - implementation of the POP3 protocol
**
** Copyright 2001-2002 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <stdlib.h>
#include <stdio.h>
#include <DataIO.h>
#include <Alert.h>
#include <Debug.h>

#include <status.h>
#include <StringList.h>
#include <ProtocolConfigView.h>

#include <MDRLanguage.h>

#include "pop3.h"
#include "md5.h"

using namespace Zoidberg;

#define POP3_RETRIEVAL_TIMEOUT 60000000
#define CRLF	"\r\n"

#define pop3_error(string) (new BAlert(MDR_DIALECT_CHOICE ("POP3 Error","POP3エラー"),string,MDR_DIALECT_CHOICE ("OK","了解"),NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go()

POP3Protocol::POP3Protocol(BMessage *settings, Mail::StatusView *status)
	: Mail::SimpleProtocol(settings,status),
	fNumMessages(-1),
	fMailDropSize(0)
{
	Init();
}


POP3Protocol::~POP3Protocol()
{
	SendCommand("QUIT" CRLF);

	conn.Close();
}


void POP3Protocol::SetStatusReporter(Mail::StatusView *view)
{
	status_view = view;
}


status_t POP3Protocol::Open(const char *server, int port, int)
{
	status_view->SetMessage(MDR_DIALECT_CHOICE ("Connecting to POP3 Server...","POP3サーバに接続しています..."));

	if (port <= 0)
		port = 110;

	fLog = "";
	
	//-----Prime the error message
	BString error_msg;
	error_msg << MDR_DIALECT_CHOICE ("Error while connecting to server ","サーバに接続中にエラーが発生しました ") << server;
	if (port != 110)
		error_msg << ":" << port;

	status_t err;	
	err = conn.Connect(server, port);

	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (": Connection refused or host not found",": ：接続が拒否されたかサーバーが見つかりません");
		pop3_error(error_msg.String());
		
		return err;
	}
	
	BString line;
	if( ReceiveLine(line) <= 0) {
		error_msg << ": " << conn.ErrorStr();
		pop3_error(error_msg.String());		
		
		return B_ERROR;
	}
	
	if(strncmp(line.String(),"+OK",3) != 0) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << line.String();
		pop3_error(error_msg.String());
			
		return B_ERROR;	
	}
	
	fLog = line;

	return B_OK;
}


status_t POP3Protocol::Login(const char *uid, const char *password, int method)
{
	status_t err;
	
	BString error_msg;
	error_msg << MDR_DIALECT_CHOICE ("Error while authenticating user ","ユーザー認証中にエラーが発生しました ") << uid;
	
	if (method == 1) {	//APOP
		int32 index = fLog.FindFirst("<");
		if(index != B_ERROR) {
			status_view->SetMessage(MDR_DIALECT_CHOICE ("Sending APOP authentication...","APOP認証情報を送信中..."));
			int32 end = fLog.FindFirst(">",index);
			BString timestamp("");
			fLog.CopyInto(timestamp,index,end-index+1);
			timestamp += password;
			char md5sum[33];
			MD5Digest((unsigned char*)timestamp.String(),md5sum);
			BString cmd = "APOP ";
			cmd += uid;
			cmd += " ";
			cmd += md5sum;
			cmd += CRLF;

			err = SendCommand(cmd.String());
			if (err != B_OK) {
				error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
				pop3_error(error_msg.String());
				
				return err;
			}

			return B_OK;
		} else {
			error_msg << MDR_DIALECT_CHOICE (": The server does not support APOP.","サーバはAPOPをサポートしていません");
			pop3_error(error_msg.String());
			return B_NOT_ALLOWED;
		}
	}
	status_view->SetMessage(MDR_DIALECT_CHOICE ("Sending username...","ユーザーID送信中..."));

	BString cmd = "USER ";
	cmd += uid;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
		pop3_error(error_msg.String());
		
		return err;
	}

	status_view->SetMessage(MDR_DIALECT_CHOICE ("Sending password...","パスワード送信中..."));
	cmd = "PASS ";
	cmd += password;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
		pop3_error(error_msg.String());
		
		return err;
	}

	return B_OK;
}


status_t POP3Protocol::Stat()
{
	status_view->SetMessage(MDR_DIALECT_CHOICE ("Getting mailbox size...","メールボックスのサイズを取得しています..."));	

	if (SendCommand("STAT" CRLF) < B_OK)
		return B_ERROR;

	int32 messages,dropSize;
	if (sscanf(fLog.String(),"+OK %ld %ld",&messages,&dropSize) < 2)
		return B_ERROR;

	fNumMessages = messages;
	fMailDropSize = dropSize;

	return B_OK;
}


int32 POP3Protocol::Messages()
{
	if (fNumMessages < 0)
		Stat();

	return fNumMessages;		
}


size_t POP3Protocol::MailDropSize()
{
	if (fNumMessages < 0)
		Stat();
	
	return fMailDropSize;
}


status_t POP3Protocol::Retrieve(int32 message, BPositionIO *write_to)
{
	BString cmd;
	cmd << "RETR " << message + 1 << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, true);
}


status_t POP3Protocol::GetHeader(int32 message, BPositionIO *write_to)
{
	BString cmd;
	cmd << "TOP " << message + 1 << " 0" << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, false);
}


status_t POP3Protocol::RetrieveInternal(const char *command, int32,
	BPositionIO *write_to, bool post_progress)
{
	BString content = "";
	
	if (SendCommand(command) != B_OK)
		return B_ERROR;
		
	int32 r;
	
	char *buf = new char[1024];
	if (!buf) {
		fLog = "Memory was exhausted";
		return B_ERROR;
	}
	
	write_to->Seek(0,SEEK_SET);

	int32 content_len = 0;
	bool cont = true;
	
	while (cont) {
		if (conn.IsDataPending(POP3_RETRIEVAL_TIMEOUT)) {
			r = conn.Receive(buf, 1023);
			if (r <= 0) 
				return B_ERROR;
				
			if (post_progress)
				status_view->AddProgress(r);
				
			content_len += r;			
			buf[r] = '\0';
			content = buf;
			
			if (content_len > 5 &&
				r >= 5 &&
				buf[r-1] == '\n' && 
				buf[r-2] == '\r' &&
				buf[r-3] == '.'  &&
				buf[r-4] == '\n' &&
				buf[r-5] == '\r' ) {
					
				cont = false;
			}
				
			//content.ReplaceAll("\n..","\n.");
			write_to->Write(buf,r);
			
			if ((content_len > 5) && (r < 5)) {
				char end[6];
				end[5] = 0;
				
				write_to->ReadAt(write_to->Position() - 5, end, 5);
				if (strcmp(end,"\r\n.\r\n") == 0)
					cont = false;		
			}
		}
	}
	
	write_to->SetSize(write_to->Position() - 5);
	
	delete[] buf;
	
	return B_OK;
}


status_t POP3Protocol::UniqueIDs() {
	status_t ret = B_OK;
	status_view->SetMessage(MDR_DIALECT_CHOICE ("Getting UniqueIDs...","固有のIDを取得中..."));
	
	ret = SendCommand("UIDL" CRLF);
	if (ret != B_OK) return ret;
		
	BString result;
	int32 uid_offset;
	while (ReceiveLine(result) > 0) {
		if (result.ByteAt(0) == '.')
			break;
			
		uid_offset = result.FindFirst(' ') + 1;
		result.Remove(0,uid_offset);
		unique_ids->AddItem(result.String());
	}
	
	return ret;
}


void POP3Protocol::Delete(int32 num) {
	BString cmd = "DELE ";
	cmd << (num+1) << CRLF;
	if (SendCommand(cmd.String()) != B_OK) {
		// Error
	}
	#if DEBUG
	 puts(fLog.String());
	#endif
}


size_t POP3Protocol::MessageSize(int32 index) {
	BString cmd = "LIST ";
	cmd << (index+1) << CRLF;
	if (SendCommand(cmd.String()) != B_OK)
		return 0;
	int32 i = fLog.FindLast(" ");
	if (i >= 0)
		return atol(&(fLog.String()[i]));
	return 0;	
}


int32 POP3Protocol::ReceiveLine(BString &line) {
	int32 len = 0,rcv;
	int8 c = 0;
	bool flag = false;

	line = "";
	
	if (conn.IsDataPending(POP3_RETRIEVAL_TIMEOUT)) {	
		while (true) { // Hope there's an end of line out there else this gets stuck.
			rcv = conn.Receive(&c,1);
			if (rcv < 0)
				return conn.Error(); //--An error!
			if((c == '\n') || (rcv == 0 /* EOF */))
				break;

			if (c == '\r') {
				flag = true;
			} else {
				if (flag) {
					len++;
					line += '\r';
					flag = false;
				}
				len++;
				line += (char)c;
			}
		}
	} else {
		fLog = "POP3 socket timeout.";
	}
	return len;
}


status_t POP3Protocol::SendCommand(const char* cmd) {
	int32 len;

	if (conn.Send(cmd, ::strlen(cmd)) < 0)
		return conn.Error();

	fLog="";
	status_t err = B_OK;
	
	while(true) {
		len = ReceiveLine(fLog);
		if(len <= 0|fLog.ICompare("+OK",3) == 0)
			break;
			
		else if(fLog.ICompare("-ERR",4) == 0) {
			err = B_ERROR;
			break;
		} else
			return B_BAD_VALUE; //-------If it's not +OK, and it's not -ERR, then what the heck is it? Presume an error
	}
	return err;
}


void POP3Protocol::MD5Digest (unsigned char *in,char *ascii_digest)
{
	int i;
	MD5_CTX context;
	unsigned char digest[16];
	
	MD5Init(&context);
	MD5Update(&context, in, ::strlen((char*)in));
	MD5Final(digest, &context);
  	
  	for (i = 0;  i < 16;  i++) 
    	sprintf(ascii_digest+2*i, "%02x", digest[i]);
    	
	return;
}


Mail::Filter *instantiate_mailfilter(BMessage *settings, Mail::StatusView *view)
{
	return new POP3Protocol(settings,view);
}


BView* instantiate_config_panel(BMessage *settings,BMessage *)
{
	Mail::ProtocolConfigView *view = new Mail::ProtocolConfigView(Mail::MP_HAS_USERNAME | Mail::MP_HAS_AUTH_METHODS | Mail::MP_HAS_PASSWORD | Mail::MP_HAS_HOSTNAME | Mail::MP_CAN_LEAVE_MAIL_ON_SERVER);
	view->AddAuthMethod("Plain Text");
	view->AddAuthMethod("APOP");
	
	view->SetTo(settings);
	
	return view;
}
