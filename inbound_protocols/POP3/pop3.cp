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
#include <ChainRunner.h>

#include <MDRLanguage.h>

#include "pop3.h"
#include "md5.h"

using namespace Zoidberg;

#define POP3_RETRIEVAL_TIMEOUT 60000000
#define CRLF	"\r\n"

#define pop3_error(string) runner->ShowError(string)

POP3Protocol::POP3Protocol(BMessage *settings, Mail::ChainRunner *status)
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


status_t POP3Protocol::Open(const char *server, int port, int)
{
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Connecting to POP3 Server...","POP3サーバに接続しています..."));

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
			runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending APOP authentication...","APOP認証情報を送信中..."));
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
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending username...","ユーザーID送信中..."));

	BString cmd = "USER ";
	cmd += uid;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
		pop3_error(error_msg.String());
		
		return err;
	}

	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending password...","パスワード送信中..."));
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
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Getting mailbox size...","メールボックスのサイズを取得しています..."));	

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
	status_t returnCode;
	BString cmd;
	cmd << "RETR " << message + 1 << CRLF;
	returnCode = RetrieveInternal(cmd.String(), message, write_to, true);
	runner->ReportProgress(0 /* bytes */, 1 /* messages */);

	if (returnCode == B_OK) { // Some debug code.
		int32 message_len = MessageSize(message);
 		write_to->Seek (0, SEEK_END);
		if (write_to->Position() != message_len) {
			printf ("POP3Protocol::Retrieve Bug!  Message size is %d, was "
			"expecting %ld, for message #%ld.\n",
			(int) write_to->Position(), message_len, message);
		}
	}

	return returnCode;

#if 0
    // Occasional bugs with this code, sometimes see "+OK 2881 octets" text in message.
	int32 message_len = MessageSize(message);
	if (SendCommand(cmd.String()) != B_OK)
		return B_ERROR;
	int32 octets_read = 0;
	int32 temp;
	char *buffer = new char[message_len];

	while (octets_read < message_len) {
		temp = conn.Receive(buffer + octets_read,message_len - octets_read);
		if (temp < 0)
			return conn.Error();
		if (temp == 0)
			return -1; // Shouldn't happen, but...
		runner->ReportProgress(temp,0);
		octets_read += temp;
	}
	write_to->Write(buffer,message_len);
	conn.Receive(buffer,5);
	delete [] buffer;
	runner->ReportProgress(0,1);
	return B_OK;
#endif
}


status_t POP3Protocol::GetHeader(int32 message, BPositionIO *write_to)
{
	BString cmd;
	cmd << "TOP " << message + 1 << " 0" << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, false);
}


status_t POP3Protocol::RetrieveInternal(const char *command, int32 message,
	BPositionIO *write_to, bool post_progress)
{
	const int bufSize = 10240;

	// To avoid waiting for the non-arrival of the next data packet, try to
	// receive only the message size, plus the 3 extra bytes for the ".\r\n"
	// after the message.  Of course, if we get it wrong (or it is a huge
	// message), it will then switch back to receiving full buffers until the
	// message is done.
	int amountToReceive = MessageSize (message) + 3;
	if (amountToReceive >= bufSize || amountToReceive <= 0)
		amountToReceive = bufSize - 1;
	
	int32 r;
	BString bufBString; // Used for auto-dealloc on return feature.
	char *buf = bufBString.LockBuffer (bufSize);
	int32 content_len = 0;
	bool cont = true;
	write_to->Seek(0,SEEK_SET);

	if (SendCommand(command) != B_OK)
		return B_ERROR;

	while (cont) {
		if (conn.IsDataPending(POP3_RETRIEVAL_TIMEOUT)) {
			r = conn.Receive(buf, amountToReceive);
			amountToReceive = bufSize - 1;
			if (r < 0)
				return conn.Error();
			if (r == 0)
				return B_ERROR; // Shouldn't happen, but...

			if (post_progress)
				runner->ReportProgress(r,0);
				
			content_len += r;			
			buf[r] = '\0';
			
			if (r >= 5 &&
				buf[r-1] == '\n' && 
				buf[r-2] == '\r' &&
				buf[r-3] == '.'  &&
				buf[r-4] == '\n' &&
				buf[r-5] == '\r' )
				cont = false;
				
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

	write_to->SetSize(write_to->Position() - 3 /* All but the last ".\r\n" */);
	return B_OK;
}


status_t POP3Protocol::UniqueIDs() {
	status_t ret = B_OK;
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Getting UniqueIDs...","固有のIDを取得中..."));
	
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
	
	if (SendCommand("LIST"CRLF) != B_OK)
		return B_ERROR;
	
	int32 b;
	while (ReceiveLine(result) > 0) {
		if (result.ByteAt(0) == '.')
			break;
			
		b = result.FindLast(" ");
		if (b >= 0)
			b = atol(&(result.String()[b]));
		else
			b = 0;
		sizes.AddItem((void *)(b));
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
	return (size_t)(sizes.ItemAt(index));
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

	// Flush any accumulated garbage data before we send our command, so we
	// don't misinterrpret responses from previous commands (that got left over
	// due to bugs) as being from this command.

	while (conn.IsDataPending(1000 /* very short timeout, hangs with 0 in R5 */)) {
		int amountReceived;
		char tempString [1025];
		amountReceived = conn.Receive (tempString, sizeof (tempString) - 1);
		if (amountReceived < 0)
			return conn.Error();
		tempString [amountReceived] = 0;
		printf ("POP3Protocol::SendCommand Bug!  Had to flush %d bytes: %s\n",
			amountReceived, tempString);
		if (amountReceived == 0)
			break;
	}

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


Mail::Filter *instantiate_mailfilter(BMessage *settings, Mail::ChainRunner *runner)
{
	return new POP3Protocol(settings,runner);
}


BView* instantiate_config_panel(BMessage *settings,BMessage *)
{
	Mail::ProtocolConfigView *view = new Mail::ProtocolConfigView(Mail::MP_HAS_USERNAME | Mail::MP_HAS_AUTH_METHODS | Mail::MP_HAS_PASSWORD | Mail::MP_HAS_HOSTNAME | Mail::MP_CAN_LEAVE_MAIL_ON_SERVER);
	view->AddAuthMethod("Plain Text");
	view->AddAuthMethod("APOP");
	
	view->SetTo(settings);
	
	return view;
}
