#include <stdlib.h>
#include <stdio.h>
#include <DataIO.h>
#include <Alert.h>
#include <Debug.h>

#include <status.h>
#include <StringList.h>
#include <ProtocolConfigView.h>

#include "pop3.h"
#include "md5.h"

#define CRLF	"\r\n"
#define pop3_error(string) (new BAlert("POP3 Error",string,"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go()

POP3Protocol::POP3Protocol(BMessage *settings, StatusView *status) : SimpleMailProtocol(settings,status) {
	Init();
}

void POP3Protocol::SetStatusReporter(StatusView *view) {
	status_view = view;
}

status_t POP3Protocol::Open(const char *server, int port, int) {
	status_view->SetMessage("Connecting to POP3 Server...");

	if (port <= 0)
		port = 110;

	fLog = "";
	
	//-----Prime the error message
	BString error_msg;
	error_msg << "Error while connecting to server " << server;
	if (port != 110)
		error_msg << ":" << port;

	status_t err;	
	err = conn.Connect(server, port);

	if (err != B_OK) {
		error_msg << ": Connection refused or host not found";
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
		error_msg << ". The server said:\n" << line.String();
		pop3_error(error_msg.String());
			
		return B_ERROR;	
	}
	
	fLog = line;

	return B_OK;
}

status_t POP3Protocol::Login(const char *uid, const char *password, int method) {
	status_t err;
	
	BString error_msg;
	error_msg << "Error while authenticating user " << uid;
	
	if (method == 1) {	//APOP
		int32 index = fLog.FindFirst("<");
		if(index != B_ERROR) {
			status_view->SetMessage("Sending APOP authentication...");		
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
				error_msg << ". The server said:\n" << fLog;
				pop3_error(error_msg.String());
				
				return err;
			}

			return B_OK;
		} else {
			error_msg << ": The server does not support APOP.";
			pop3_error(error_msg.String());
			return B_NOT_ALLOWED;
		}
	}
	status_view->SetMessage("Sending username...");

	BString cmd = "USER ";
	cmd += uid;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if(err != B_OK) {
		error_msg << ". The server said:\n" << fLog;
		pop3_error(error_msg.String());
		
		return err;
	}

	status_view->SetMessage("Sending password...");
	cmd = "PASS ";
	cmd += password;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << ". The server said:\n" << fLog;
		pop3_error(error_msg.String());
		
		return err;
	}

	return B_OK;
}

POP3Protocol::~POP3Protocol() {
	BString cmd = "QUIT";
	cmd += CRLF;
	
	if( SendCommand(cmd.String()) != B_OK) {
		// Error
	}

	conn.Close();
}

int32 POP3Protocol::Messages() {
	status_view->SetMessage("Getting number of messages...");	
	int32 mails;

	BString cmd = "STAT";
	cmd += CRLF;
	if( SendCommand(cmd.String()) != B_OK)
		return -1;
	
	const char* log = fLog.String();
	
	mails = atol(&log[4]);
	return mails;
}

size_t POP3Protocol::MailDropSize() {
	status_view->SetMessage("Getting mailbox size...");

	if( SendCommand("STAT"CRLF) != B_OK)
		return 0;
	
	int32 i = fLog.FindLast(" ");
	const char* log = fLog.String();
	
	if (i >= 0)
		return atol(&log[i]);
	return 0;
}

status_t POP3Protocol::Retrieve(int32 message, BPositionIO *write_to) {
	BString cmd;
	cmd << "RETR " << message + 1 << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, true);
}

status_t POP3Protocol::GetHeader(int32 message, BPositionIO *write_to) {
	BString cmd;
	cmd << "TOP " << message + 1 << " 0" << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, false);
}

status_t POP3Protocol::RetrieveInternal(const char *command, int32, BPositionIO *write_to, bool post_progress) {
	BString content = "";
	
	if( SendCommand(command) != B_OK)
		return B_ERROR;
		
	int32 r;
	
	char *buf = new char[1024];
	if(!buf) {
		fLog = "Memory was exhausted";
		return B_ERROR;
	}
	
	write_to->Seek(0,SEEK_SET);

	int32 content_len = 0;
	bool cont = true;
	
	while(cont) {
		if(conn.IsDataPending(60000000)) {
			r = conn.Receive(buf, 1023);
			if(r <= 0) 
				return B_ERROR;
				
			if (post_progress)
				status_view->AddProgress(r);
				
			content_len += r;			
			buf[r] = '\0';
			content = buf;
			
			if(	content_len > 5 &&
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
	status_view->SetMessage("Getting UniqueIDs...");
	
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
	
	if (conn.IsDataPending(60000000)) {	
		while (true) {
			rcv = conn.Receive(&c,1);
			if((rcv <=0) || (c == '\n') || (c == EOF))
				break;

			if (c == '\r') {
				flag = true;
			} else {
				if (flag) {
					len++;
					line += '\r';
					flag = false;
				}
				len += rcv;
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

	if (conn.Send(cmd, ::strlen(cmd)) == B_ERROR)
		return B_ERROR;

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

MailFilter *instantiate_mailfilter(BMessage *settings, StatusView *view) {
	return new POP3Protocol(settings,view);
}

BView* instantiate_config_panel(BMessage *settings,BMessage *) {
	ProtocolConfigView *view = new ProtocolConfigView(Z_HAS_USERNAME | Z_HAS_AUTH_METHODS | Z_HAS_PASSWORD | Z_HAS_HOSTNAME | Z_CAN_LEAVE_MAIL_ON_SERVER);
	view->AddAuthMethod("Plain Text");
	view->AddAuthMethod("APOP");
	
	view->SetTo(settings);
	
	return view;
}
