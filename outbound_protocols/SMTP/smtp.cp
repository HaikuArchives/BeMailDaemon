#include <DataIO.h>
#include <Message.h>
#include <Alert.h>
#include <TextControl.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>

#include <status.h>
#include <ProtocolConfigView.h>

#include "smtp.h"

#define CRLF "\r\n"
#define smtp_errlert(string) (new BAlert("SMTP Error",string,"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();

SMTPProtocol::SMTPProtocol(BMessage *message, StatusView *view) :
	MailFilter(message),
	_settings(message),
	status_view(view),
	err(B_OK) {
		BString error_msg;
			
		err = Open(_settings->FindString("server"),_settings->FindInt32("port"));
		if (err < B_OK) {
			error_msg << "Error while opening connection to " << _settings->FindString("server");
			
			if (_settings->FindInt32("port") > 0)
				error_msg << ":" << _settings->FindInt32("port");
				
			// << strerror(err) - BNetEndpoint sucks, we can't use this;
			if (fLog.Length() > 0)
				error_msg << ". The server says:\n" << fLog;
			else
				error_msg << ": Connection refused or host not found.";
			
			smtp_errlert(error_msg.String());
			return;
		}
			
		err = Login(_settings->FindString("username"),_settings->FindString("password"),_settings->FindInt32("auth_method"));
		if (err < B_OK) {
			//-----This is a really cool kind of error message. How can we make it work for POP3?
			error_msg << "Error while logging in to " << _settings->FindString("server") << ". The server said:\n" << fLog;
			smtp_errlert(error_msg.String());
		}
			
	}
		
status_t SMTPProtocol::InitCheck(BString *verbose) {
	if (verbose != NULL)
		*verbose << "Error while fetching mail from " << _settings->FindString("server") << ": " << strerror(err);
	return err;
}

MDStatus SMTPProtocol::ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	) {
		if (Send(io_headers->FindString("MAIL:recipients"),io_headers->FindString("MAIL:from"),*io_message) == B_OK) {
			status_view->AddItem();
			return MD_HANDLED;
		} else {
			BString error;
			error << "An error occurred while sending the message " << io_headers->FindString("MAIL:subject") << " to " << io_headers->FindString("MAIL:recipients") << ":\n" << fLog;
			smtp_errlert(error.String());
			status_view->AddItem();
			return MD_ERROR;
		}
	}

status_t SMTPProtocol::Open(const char *server, int port) {
	status_view->SetMessage("Connecting to server...");

	if (port <= 0)
		port = 25;
		
	fLog = "";
		
	status_t err;	
	err = conn.Connect(server, port);
		
	if (err != B_OK)
		return err;
				
	BString line;
	
	if (ReceiveLine(line) <= 0)
		return conn.Error();
	
	BString cmd;

	cmd = "HELO "; 
	cmd << server << CRLF;
	if (SendCommand(cmd.String()) != B_OK)
		return B_ERROR;
	
	return B_OK;

}

status_t SMTPProtocol::Login(const char *, const char *, int ) {
	return B_OK;
}

void SMTPProtocol::Close() {
	status_view->SetMessage("Closing connection...");
	
	BString cmd = "QUIT";
	cmd += CRLF;
	
	if( SendCommand(cmd.String()) != B_OK) {
		// Error
	}

	conn.Close();
}

status_t SMTPProtocol::Send(const char *to, const char *from, BPositionIO *message) {
	BString cmd = from;
	cmd.Remove(0,cmd.FindFirst("\" <") + 2);
	cmd.Prepend("MAIL FROM: ");
	cmd += CRLF;
	if (SendCommand(cmd.String()) != B_OK)
		return B_ERROR;

	int32 len = strlen(to);
	BString addr("");
	for(int32 i = 0;i < len;i++) {
		char c = to[i];
		if(c != ',')
			addr += (char)c;
		if(c == ','||i == len-1) {
			if(addr.Length() == 0)
				continue;
			cmd = "RCPT TO: ";
			cmd << addr.String() << CRLF;
			if(SendCommand(cmd.String()) != B_OK)
				return B_ERROR;

			addr ="";
		}
	}

	cmd = "DATA";
	cmd += CRLF;
	if(SendCommand(cmd.String()) != B_OK)
		return B_ERROR;						
	
	BString line("");
		
	int32 sent_len = 0;
	int32 front = 0;
	message->Seek(0,SEEK_END);
	len = message->Position();
	message->Seek(0,SEEK_SET);
	char *data = new char[500];
	while (sent_len < len) {
		front = 500;
		if ((front + sent_len) > len)
			front = len - sent_len;
			
		message->Read(data,front);
		conn.Send(data,front);
		sent_len += front;
		status_view->AddProgress(front);
	}
	delete [] data;

	cmd = CRLF"."CRLF;
	if( SendCommand(cmd.String()) != B_OK)
		return B_ERROR;

	return B_OK;
} 

int32 SMTPProtocol::ReceiveLine(BString &line) {
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
		fLog = "SMTP socket timeout.";
	}
	return len;
}

status_t SMTPProtocol::SendCommand(const char* cmd) {
	int32 len;
	puts(cmd);
	
 	if (conn.Send(cmd, ::strlen(cmd)) == B_ERROR)
		return B_ERROR;
		
	fLog = "";

	while(true) {
		len = ReceiveLine(fLog);
		
		if(len <= 0)
			return B_ERROR;
			
		if(fLog.Length() > 4 && (fLog[3] == ' ' || fLog[3] == '-')) {
			const char* top = fLog.String();
			int32 num = atol (top);

			if(num >= 500)
				return B_ERROR;
			else
				break;
		}
	}
	return B_OK;
}

MailFilter* instantiate_mailfilter(BMessage *settings,StatusView *status) {
	return new SMTPProtocol(settings,status);
}

BView* instantiate_config_panel(BMessage *settings) {
	ProtocolConfigView *view = new ProtocolConfigView(false,false,false,false,true,false);
	
	BTextControl *control = (BTextControl *)(view->FindView("host"));
	control->SetLabel("SMTP Host: ");
	control->SetDivider(be_plain_font->StringWidth("SMTP Host: "));
	
	view->SetTo(settings);
	
	return view;
}

