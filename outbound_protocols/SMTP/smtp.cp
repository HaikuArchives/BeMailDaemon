#include <DataIO.h>
#include <Message.h>
#include <Alert.h>
#include <TextControl.h>
#include <Entry.h>
#include <Path.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>

#include <status.h>
#include <ProtocolConfigView.h>
#include <mail_encoding.h>
#include <MailSettings.h>
#include <ChainRunner.h>
#include <crypt.h>

#include "smtp.h"
#include "md5.h"

#define CRLF "\r\n"
#define SMTP_RESPONSE_SIZE 8192
#define smtp_errlert(string) (new BAlert("SMTP Error",string,"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();


// Authentication types recognized. Not all methods are implemented.
enum AuthType{
	LOGIN=1,
	PLAIN=1<<2,
	CRAM_MD5=1<<3,
	DIGEST_MD5=1<<4
};


// Constructor. Sets up everything and call other methods.
SMTPProtocol::SMTPProtocol(BMessage *message, StatusView *view) :
	MailFilter(message),
	_settings(message),
	status_view(view),
	fAuthType(0),
	err(B_OK) 
{
	BString error_msg;
	bool esmtp = (_settings->FindInt32("auth_method") == 1);
		
	err = Open(_settings->FindString("server"),_settings->FindInt32("port"),esmtp);
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
			
	const char *password = _settings->FindString("password");
	char *passwd = get_passwd(_settings,"cpasswd");
	if (passwd)
		password = passwd;

	err = Login(_settings->FindString("username"),password);
	delete passwd;

	if (err < B_OK) {
		//-----This is a really cool kind of error message. How can we make it work for POP3?
		error_msg << "Error while logging in to " << _settings->FindString("server") << ". The server said:\n" << fLog;
		smtp_errlert(error_msg.String());
	}			
}

// Check for errors? 
status_t SMTPProtocol::InitCheck(BString *verbose) {
	if (verbose != NULL)
		*verbose << "Error while fetching mail from " << _settings->FindString("server") << ": " << strerror(err);
	return err;
}


// Process EMail to be sent
MDStatus SMTPProtocol::ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* /*io_entry*/,
		BMessage* io_headers, BPath* /*io_folder*/, BString* /*io_uid*/
	) 
{
	const char *from = io_headers->FindString("MAIL:from");
	const char *to = io_headers->FindString("MAIL:recipients");
	if (!to)
		to = io_headers->FindString("MAIL:to");

	if (to && from && Send(to,from,*io_message) == B_OK) {
		status_view->AddItem();
		return MD_HANDLED;
	} else {
		BString error;
		error << "An error occurred while sending the message " << io_headers->FindString("MAIL:subject") << " to " << to << ":\n" << fLog;
		smtp_errlert(error.String());
		status_view->AddItem();
		return MD_ERROR;
	}
}

// Opens connection to server
status_t SMTPProtocol::Open(const char* address, int port, bool esmtp)
{
	status_view->SetMessage("Connecting to server...");
	
	if (port <= 0)
		port = 25;

	status_t err;
	err = conn.Connect(address, port);
		
	if(err != B_OK)
		return err;	
	
	BString line;
	ReceiveResponse(line);
	
	char *cmd = new char[::strlen(address)+8];
	if(!esmtp)
		::sprintf(cmd,"HELO %s"CRLF, address);
	else
		::sprintf(cmd,"EHLO %s"CRLF, address);
				
	if (SendCommand(cmd) != B_OK) {
		delete[] cmd;
		return B_ERROR;
	}
	
	delete[] cmd;

	// Check auth type
	if(esmtp)
	{
		const char* res = fLog.String();
		char* p;
		if((p=::strstr(res,"250-AUTH")))
		{
			if(::strstr(p,"LOGIN"))
				fAuthType |= LOGIN;
			if(::strstr(p,"PLAIN"))
				fAuthType |= PLAIN;	
			if(::strstr(p,"CRAM-MD5"))
				fAuthType |= CRAM_MD5;
			if(::strstr(p,"DIGEST-MD5"))
				fAuthType |= DIGEST_MD5;
		}
	}
	return B_OK;
}

status_t SMTPProtocol::Login(const char* _login, const char* password)
{
	if(fAuthType == 0) {
		if (_settings->FindInt32("auth_method") == 2) {

			//*** POP3 authentification ***

			// find the POP3 filter of the other chain - identify by name...
			MailSettings mailSettings;
			BList chains;
			if (mailSettings.InboundChains(&chains) >= B_OK)
			{
				ChainRunner *parent;
				_settings->FindPointer("chain_runner",(void **)&parent);
				MailChain *chain = NULL;
				for (int i = chains.CountItems();i-- > 0;)
				{
					chain = (MailChain *)chains.ItemAt(i);
					if (chain != NULL && !strcmp(chain->Name(),parent->Chain()->Name()))
						break;
					chain = NULL;
				}
				if (chain != NULL)
				{
					// found mail chain! let's check for the POP3 protocol
					BMessage msg;
					entry_ref ref;
					if (chain->GetFilter(0,&msg,&ref) >= B_OK)
					{
						BPath path(&ref);
						if (path.InitCheck() >= B_OK/* && !strcmp(path.Leaf(),"POP3")*/)
						{
							// protocol matches, go execute it!
		
							image_id image = load_add_on(path.Path());

							fLog = "Cannot load POP3 add-on";
							if (image >= B_OK)
							{
								MailFilter *(* instantiate)(BMessage *,StatusView *);
								status_t status = get_image_symbol(image,"instantiate_mailfilter",B_SYMBOL_TYPE_TEXT,(void **)&instantiate);
								if (status >= B_OK)
								{
									ChainRunner runner(chain);
									msg.AddPointer("chain_runner",&runner);
									msg.AddInt32("chain",chain->ID());

									// instantiating and deleting should be enough
									MailFilter *filter = (*instantiate)(&msg,status_view);
									delete filter;

									return B_OK;
								}
								else
									fLog = "Cannot run POP3 add-on, symbol not found";
								unload_add_on(image);
							}
						}
					}
					else
						fLog = "Could not get inbound protocol";
				}
				else
					fLog = "Cannot find inbound chain";
					
				for (int i = chains.CountItems();i-- > 0;)
				{
					chain = (MailChain *)chains.ItemAt(i);
					delete chain;
				}
			
			}
			else
				fLog = "Cannot get inbound chains";
			return B_ERROR;
		} else
			return B_OK;
	}

	const char* login = _login;		
	char hex_digest[33];
	BString out;
	
	int32 loginlen = ::strlen(login);
	int32 passlen = ::strlen(password);

	if(fAuthType&CRAM_MD5)
	{
		//******* CRAM-MD5 Authentication ( not tested yet.)
		SendCommand("AUTH CRAM-MD5"CRLF);
		const char* res = fLog.String();
		
		if(strncmp(res,"334",3)!=0)
			return B_ERROR;
		char *base = new char[::strlen(&res[4])+1];
		int32 baselen = ::strlen(base);
		baselen = ::decode_base64(base, base, baselen);
		base[baselen] = '\0';
		
		#if DEBUG
		 printf("base: %s\n", base);
		#endif
		::MD5HexHmac(hex_digest,
				(const unsigned char*)base,
				(int)baselen,
				(const unsigned char*)password,
				(int)passlen);
		#if DEBUG
		 printf("%s\n%s\n",base,hex_digest);
		#endif
		delete[] base;
		
		char *resp = new char[(strlen(hex_digest)+loginlen)*2+3+2];
		
		::sprintf(resp,"%s %s"CRLF, login, hex_digest);
		baselen = ::encode_base64(resp,resp,strlen(resp));
		resp[baselen]='\0';
		// Hack! I'm sure there is a better way to do this
		BString t_resp(resp);
		t_resp.Append(CRLF);
		
		SendCommand(t_resp.String());
		
		delete[] resp;
		
		res = fLog.String();
		if(atol(res)<500)
			return B_OK;
		
	}
	if(fAuthType&DIGEST_MD5){
	//******* DIGEST-MD5 Authentication ( not written yet..)
		fLog = "DIGEST-MD5 Authentication is not supported";
	}	
	if(fAuthType&LOGIN){
	//******* LOGIN Authentication ( tested. works fine)
		ssize_t encodedsize; // required by our base64 implementation
		
		SendCommand("AUTH LOGIN"CRLF);
		const char* res = fLog.String();
		
		if(strncmp(res,"334",3)!=0)
			return B_ERROR;
		// Send login name as base64
		char* login64 = new char[loginlen*3+3];
		encodedsize = ::encode_base64(login64, (char *)login, loginlen);
		login64[encodedsize]='\0';		
		// Hack! I'm sure there is a better way to do this
		BString t1_login64(login64);
		t1_login64.Append(CRLF);
		
		SendCommand(t1_login64.String());
		delete [] login64;
		
		res = fLog.String();
		if(strncmp(res,"334",3)!=0)
			return B_ERROR;
		// Send password as base64
		login64 = new char[passlen*3+3];
		encodedsize = ::encode_base64(login64,(char*)password,passlen);
		login64[encodedsize]='\0';
		// Hack! I'm sure there is a better way to do this
		BString t2_login64(login64);
		t2_login64.Append(CRLF);

		SendCommand(t2_login64.String());
		delete[] login64;
		res = fLog.String();
		if(atol(res)<500)
			return B_OK;
	}
	//******* PLAIN Authentication ( not tested yet.)
	if(fAuthType&PLAIN){	
		char* login64 = new char[((loginlen+1)*2+passlen)*3];
		::memset(login64,0,((loginlen+1)*2+passlen)*3);
		::memcpy(login64,login,loginlen);
		::memcpy(login64+loginlen+1,login,loginlen);
		::memcpy(login64+loginlen*2+2,password,passlen);
		
		::encode_base64(login64,login64,((loginlen+1)*2+passlen));
		
		char *cmd = new char[strlen(login64)+12];
		::sprintf(cmd,"AUTH PLAIN %s"CRLF,login64);
		delete[] login64;
		
		SendCommand(cmd);
		delete[] cmd;	
		const char* res = fLog.String();
		if(atol(res)<500)
			return B_OK;
	}		
	return B_ERROR;
}

// Closes SMTP connection
void SMTPProtocol::Close() {
	status_view->SetMessage("Closing connection...");
	
	BString cmd = "QUIT";
	cmd += CRLF;
	
	if( SendCommand(cmd.String()) != B_OK) {
		// Error
	}

	conn.Close();
}

// Send mail
status_t SMTPProtocol::Send(const char *to, const char *from, BPositionIO *message) {
	BString cmd = from;
	cmd.Remove(0, cmd.FindFirst("\" <") + 2);
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

// Receives response from server.
int32 SMTPProtocol::ReceiveResponse(BString &out)
{
	out = "";
	int32 len = 0,r;
	char buf[SMTP_RESPONSE_SIZE];
	bigtime_t timeout = 1000000*180; //timeout 180 secs
	
	if(conn.IsDataPending(timeout))
	{	
		while(1)
		{
			r = conn.Receive(buf,SMTP_RESPONSE_SIZE-1);
			if(r <= 0)
				break;
			len += r;
			out.Append(buf,r);
			if(strstr(buf,"\r\n"))
				break;
		}
	}else{
		fLog = "SMTP socket timeout.";
	}
	#if DEBUG
	 printf("S:%s\n",out.String());
	#endif
	return len;
}

// Sends SMTP command. Result kept in fLog
status_t SMTPProtocol::SendCommand(const char* cmd)
{
	int32 len;
	#if DEBUG
	 printf("C:%s\n",cmd);
	#endif
	if (conn.Send(cmd, ::strlen(cmd)) == B_ERROR)
		return B_ERROR;
	
	fLog = "";

	// Receive
	while(1)
	{
		len = ReceiveResponse(fLog);
		
		if(len <= 0)
			return B_ERROR;
		if(fLog.Length() > 4 && (fLog[3] == ' '||fLog[3] == '-'))
		{
			const char* top = fLog.String();
			int32 num = atol( top );
			#if DEBUG
				printf("ReplyNumber: %ld\n", num);
			#endif
			if(num >= 500)
				return B_ERROR;
			else
				break;
		}
	}
	return B_OK;
}

// Instantiate hook
MailFilter* instantiate_mailfilter(BMessage *settings,StatusView *status) {
	return new SMTPProtocol(settings,status);
}

// Configuration interface
BView* instantiate_config_panel(BMessage *settings,BMessage *) {
	ProtocolConfigView *view = new ProtocolConfigView(Z_HAS_AUTH_METHODS | Z_HAS_USERNAME | Z_HAS_PASSWORD | Z_HAS_HOSTNAME);
	
	view->AddAuthMethod("None",false);
	view->AddAuthMethod("ESMTP");
	view->AddAuthMethod("POP3 before SMTP",false);

	BTextControl *control = (BTextControl *)(view->FindView("host"));
	control->SetLabel("SMTP Host: ");
	//control->SetDivider(be_plain_font->StringWidth("SMTP Host: "));
	
	view->SetTo(settings);
	
	return view;
}
