#include <List.h>
#include <String.h>
#include <Directory.h>
#include <File.h>
#include <E-mail.h>
#include <Entry.h>
#include <netdb.h>
#include <NodeInfo.h>

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>

class _EXPORT MailMessage;

#include <MailMessage.h>
#include <MailAttachment.h>
#include <MailSettings.h>
#include <MailDaemon.h>

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."

MailMessage::MailMessage(BPositionIO *mail_file)
            :_bcc(NULL)
            ,_num_components(0)
            ,_body(NULL)
            , _text_body(NULL)
            ,_header_kludge_yikes(NULL)
{
	MailSettings settings;
	_chain_id = settings.DefaultOutboundChainID();
	
	if (mail_file == NULL) {
		_header_kludge_yikes = new BMessage;
		return;
	}
		
	MailComponent headers;
	mail_file->Seek(0,SEEK_END);
	size_t length = mail_file->Position();
	mail_file->Seek(0,SEEK_SET);
	
	headers.Instantiate(mail_file,length);
	_body = headers.WhatIsThis();
	
	mail_file->Seek(0,SEEK_SET);
	_body->Instantiate(mail_file,length);
	
	MIMEMultipartContainer *cont = dynamic_cast<MIMEMultipartContainer *> (_body);
	
	_num_components = (cont == NULL) ? 1 : cont->CountComponents();
	
	_text_body = dynamic_cast<PlainTextBodyComponent *> (_body);
	
	if ((_text_body == NULL) && (cont != NULL)) {
		MailComponent component, *it_is;
		for (int32 i = 0; i < cont->CountComponents(); i++) {
			cont->ManualGetComponent(&component,i);
			it_is = component.WhatIsThis();
			_text_body = dynamic_cast<PlainTextBodyComponent *> (it_is);
			
			if (_text_body != NULL)
				break;
			else
				delete it_is;
		}
	}
}

MailMessage::~MailMessage() {
	if (_bcc != NULL)
		free(_bcc);
		
	if (_body != NULL)
		delete _body;
		
	if (_header_kludge_yikes != NULL)
		delete _header_kludge_yikes;
}

const char *MailMessage::To() {
	return HeaderField("To");
}

const char *MailMessage::From() {
	return HeaderField("From");
}

const char *MailMessage::ReplyTo() {
	return HeaderField("Reply-To");
}

const char *MailMessage::CC() {
	return HeaderField("Cc");
}

const char *MailMessage::Subject() {
	return HeaderField("Subject");
}

void MailMessage::SetSubject(const char *subject) {
	SetHeaderField("Subject",subject);
}

void MailMessage::SetReplyTo(const char *reply_to) {
	SetHeaderField("Reply-To",reply_to);
}

void MailMessage::SetFrom(const char *from) {
	SetHeaderField("From",from);
}

void MailMessage::SetTo(const char *to) {
	SetHeaderField("To",to);
}

void MailMessage::SetCC(const char *cc) {
	SetHeaderField("CC",cc);
}

void MailMessage::SetBCC(const char *bcc) {
	if (_bcc != NULL)
		free(_bcc);
		
	_bcc = strdup(bcc);
}

const char *MailMessage::HeaderField(const char *name) {
	if (_header_kludge_yikes != NULL) {
		if (_header_kludge_yikes->HasString(name))
			return _header_kludge_yikes->FindString(name);
	}
	
	if (_body == NULL)
		return NULL;
		
	return _body->HeaderField(name);
}
		
 void MailMessage::SetHeaderField(const char *name, const char *with) {
	if (_header_kludge_yikes != NULL) {
		if (_header_kludge_yikes->ReplaceString(name,with) != B_OK)
			_header_kludge_yikes->AddString(name,with);
			
		return;
	}
	
	if (_body != NULL)
		_body->AddHeaderField(name,with);
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

int32 MailMessage::CountComponents() {
	return _num_components;
}

void MailMessage::Attach(entry_ref *ref, bool include_attachments) {
	if (include_attachments) {
		AddComponent(new AttributedMailAttachment(ref));
	} else {
		SimpleMailAttachment *attach = new SimpleMailAttachment;
		attach->SetFileName(ref->name);
		
		BFile *file = new BFile(ref,B_READ_ONLY);
		attach->SetDecodedDataAndDeleteWhenDone(file);
		
		AddComponent(attach);
	}
}
		
bool MailMessage::IsComponentAttachment(int32 i) {
	if ((i >= _num_components) || (_num_components == 0))
		return false;
		
	if (_num_components == 1)
		return _body->IsAttachment();
		
	MailComponent component;
	((MIMEMultipartContainer *)(_body))->ManualGetComponent(&component,i);
	return component.IsAttachment();
}

void MailMessage::SetBodyTextTo(const char *text) {
	if (_text_body == NULL) {
		_text_body = new PlainTextBodyComponent;
		AddComponent(_text_body);
	}
	
	_text_body->SetText(text);
}	
	
const char *MailMessage::BodyText() {
	if (_text_body == NULL)
		return NULL;
		
	return _text_body->Text();
}


status_t MailMessage::SetBody(PlainTextBodyComponent *body) {
	if (_text_body != NULL) {
		return B_ERROR;
//	removing doesn't exist for now
//		RemoveComponent(_text_body);
//		delete _text_body;
	}
	_text_body = body;
	AddComponent(_text_body);

	return B_OK;
}


status_t MailMessage::RenderTo(BFile *file) {
	// FIXME: please return an apropriate error code
	if (_body == NULL) {
		puts("no body!");
		return B_ERROR;
	}

	//------Copy in headers from _header_kludge_yikes
	if (_header_kludge_yikes != NULL) {
		type_code type;

#ifdef B_BEOS_VERSION_DANO
		const
#endif
		char *name;
		const char *data;
		
		for (int32 i = 0; _header_kludge_yikes->GetInfo(B_STRING_TYPE,i,&name,&type) >= B_OK; i++) {
			if (_header_kludge_yikes->FindString(name,&data) >= B_OK)
				_body->AddHeaderField(name,data);
		}
		
		delete _header_kludge_yikes;
		_header_kludge_yikes = NULL;
	}
	
	//------Do real rendering
	
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
	
	BNodeInfo(file).SetType(B_MAIL_TYPE);
	
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
	
	return _body->Render(file);
}
	
status_t MailMessage::RenderTo(BDirectory *dir) {
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
	
	return RenderTo(&file);
}
	
status_t MailMessage::Send(bool send_now) {
	MailChain *via = new MailChain(_chain_id);
	if ((via->InitCheck() != B_OK) || (via->ChainDirection() != outbound)) {
		delete via;
		via = new MailChain(MailSettings().DefaultOutboundChainID());
	}
	
	create_directory(via->MetaData()->FindString("path"),0777);
	BDirectory dir(via->MetaData()->FindString("path"));

	status_t status = RenderTo(&dir);
	if (status >= B_OK && send_now)
		status = MailDaemon::SendQueuedMail();
		
	delete via;
	
	return status;
}


