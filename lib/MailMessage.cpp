#include <List.h>
#include <String.h>
#include <Directory.h>
#include <File.h>
#include <E-mail.h>
#include <netdb.h>

#include <malloc.h>
#include <string.h>
#include <sys/utsname.h>

class _EXPORT MailMessage;

#include <MailMessage.h>
#include <MailSettings.h>
#include <MailDaemon.h>

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."

MailMessage::MailMessage(BPositionIO *mail_file) : _num_components(0), _body(NULL), _bcc(NULL) {
	MailSettings settings;
	_chain_id = settings.DefaultOutboundChainID();
	
	if (mail_file == NULL)
		return;
		
	MailComponent headers;
	mail_file->Seek(0,SEEK_END);
	size_t length = mail_file->Position();
	mail_file->Seek(0,SEEK_SET);
	
	headers.Instantiate(mail_file,length);
	_body = WhatIsThis(&headers);
	
	mail_file->Seek(0,SEEK_SET);
	_body->Instantiate(mail_file,length);
	
	MIMEMultipartContainer *cont = dynamic_cast<MIMEMultipartContainer *> (_body);
	
	_num_components = (cont == NULL) ? 1 : cont->CountComponents();
}

const char *MailMessage::To() {
	return _body->HeaderField("To");
}

const char *MailMessage::From() {
	return _body->HeaderField("From");
}

const char *MailMessage::ReplyTo() {
	return _body->HeaderField("Reply-To");
}

const char *MailMessage::CC() {
	return _body->HeaderField("Cc");
}

const char *MailMessage::Subject() {
	return _body->HeaderField("Subject");
}

void MailMessage::SetSubject(const char *subject) {
	_body->AddHeaderField("Subject",subject);
}

void MailMessage::SetReplyTo(const char *reply_to) {
	_body->AddHeaderField("Reply-To",reply_to);
}

void MailMessage::SetFrom(const char *from) {
	_body->AddHeaderField("From",from);
}

void MailMessage::SetTo(const char *to) {
	_body->AddHeaderField("To",to);
}

void MailMessage::SetCC(const char *cc) {
	_body->AddHeaderField("CC",cc);
}

void MailMessage::SetBCC(const char *bcc) {
	if (_bcc != NULL)
		free(_bcc);
		
	_bcc = strdup(bcc);
}

void MailMessage::SendViaAccount(const char *account_name) {
	BList chains;
	MailSettings::OutboundChains(&chains);
	
	for (int32 i = 0; i < chains.CountItems(); i++) {
		if (strcmp(((MailChain *)(chains.ItemAt(i)))->Name(),account_name) == 0) {
			SendViaAccount(((MailChain *)(chains.ItemAt(i)))->ID());
			break;
		}
	}
	
	while (chains.CountItems() > 0)
		delete chains.RemoveItem(0L);
}
	
void MailMessage::SendViaAccount(int32 chain_id) {
	_chain_id = chain_id;
	
	MailChain chain(_chain_id);
	BString from;
	from << '\"' << chain.MetaData()->FindString("real_name") << "\" <" << chain.MetaData()->FindString("reply_to") << '>';
	SetFrom(from.String());
}

void MailMessage::AddComponent(MailComponent *component) {
	if (_num_components == 0)
		_body = component;
	else if (_num_components == 1) {
		MIMEMultipartContainer *container = new MIMEMultipartContainer(mime_boundary,mime_warning);
		container->AddComponent(_body);
		container->AddComponent(component);
		_body = container;
	} else {
		((MIMEMultipartContainer *)(_body))->AddComponent(component);
	}
	
	_num_components++;
}

MailComponent *MailMessage::GetComponent(int32 i) {
	if (_num_components <= 1) {
		if (i >= _num_components)
			return NULL;
		else return _body;
	} else {
		return ((MIMEMultipartContainer *)(_body))->GetComponent(i);
	}
	
	return NULL;
}

void MailMessage::RenderTo(BFile *file) {
	if (From() == NULL)
		SendViaAccount(_chain_id); //-----Set the from string
	
	BString recipients;
	recipients << To();
	if ((CC() != NULL) && (strlen(CC()) > 0))
		recipients << ',' << CC();
	if ((_bcc != NULL) && (strlen(_bcc) > 0))
		recipients << ',' << _bcc;
		
	file->WriteAttrString(B_MAIL_ATTR_RECIPIENTS,&recipients);
	
	BString attr;
	
	attr = To();
	file->WriteAttrString(B_MAIL_ATTR_TO,&attr);
	attr = CC();
	file->WriteAttrString(B_MAIL_ATTR_CC,&attr);
	attr = Subject();
	file->WriteAttrString(B_MAIL_ATTR_SUBJECT,&attr);
	attr = ReplyTo();
	file->WriteAttrString(B_MAIL_ATTR_REPLY,&attr);
	attr = From();
	file->WriteAttrString(B_MAIL_ATTR_FROM,&attr);
	attr = "Pending";
	file->WriteAttrString(B_MAIL_ATTR_STATUS,&attr);
	attr = "1.0";
	file->WriteAttrString(B_MAIL_ATTR_MIME,&attr);
	attr = MailChain(_chain_id).Name();
	file->WriteAttrString("MAIL:account",&attr);
	
	int32 int_attr;
	
	int_attr = time(NULL);
	file->WriteAttr(B_MAIL_ATTR_WHEN,B_TIME_TYPE,0,&int_attr,sizeof(int32));
	int_attr = B_MAIL_PENDING | B_MAIL_SAVE;
	file->WriteAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&int_attr,sizeof(int32));
	
	file->WriteAttr("MAIL:chain",B_INT32_TYPE,0,&_chain_id,sizeof(int32));
	
	/* add a message-id */
	BString message_id;
	/* empirical evidence indicates message id must be enclosed in
	** angle brackets and there must be an "at" symbol in it
	*/
	message_id << "<";
	message_id << system_time();
	message_id << "-BeMail@";
	
	#if BONE
		utsname uinfo;
		uname(&uinfo);
		message_id << uinfo.nodename;
	#else
		char host[255];
		gethostname(host,255);
		message_id << host;
	#endif

	message_id << ">";
	_body->AddHeaderField("Message-ID: ", message_id.String());
	
	_body->Render(file);
}
	
void MailMessage::RenderTo(BDirectory *dir) {
	BString name;
	name << "\"" << Subject() << "\": <" << To() << ">";
	
	BString worker;
	int32 uniquer = time(NULL);
	worker = name;
	
	while (dir->Contains(worker.String())) {
		worker = name;
		uniquer++;
		
		worker << ' ' << uniquer;
	}
		
	BFile file;
	dir->CreateFile(worker.String(),&file);
	
	RenderTo(&file);
}
	
void MailMessage::Send(bool send_now) {
	MailChain *via = new MailChain(_chain_id);
	if ((via->InitCheck() != B_OK) || (via->ChainDirection() != outbound)) {
		delete via;
		via = new MailChain(MailSettings().DefaultOutboundChainID());
	}
	
	BDirectory dir(via->MetaData()->FindString("path"));
	RenderTo(&dir);
	
	if (send_now)
		MailDaemon::SendQueuedMail();
		
	delete via;
}
		
	