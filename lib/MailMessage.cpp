#include <List.h>
#include <String.h>
#include <Directory.h>
#include <File.h>
#include <E-mail.h>
#include <Entry.h>
#include <netdb.h>
#include <NodeInfo.h>
#include <ClassInfo.h>

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <ctype.h>

class _EXPORT MailMessage;

#include <MailMessage.h>
#include <MailAttachment.h>
#include <MailSettings.h>
#include <MailDaemon.h>
#include <StringList.h>

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."

MailMessage::MailMessage(BPositionIO *mail_file)
            :_status(B_NO_ERROR)
			,_chain_id(0)
            ,_bcc(NULL)
            ,_num_components(0)
            ,_body(NULL)
            ,_text_body(NULL)
{
	MailSettings settings;
	_chain_id = settings.DefaultOutboundChainID();
		
	if (mail_file != NULL)
		SetToRFC822(mail_file,-1);
}

MailMessage::~MailMessage() {
	if (_bcc != NULL)
		free(_bcc);
		
	delete _body;
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
	return HeaderField("CC");
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

status_t MailMessage::AddComponent(MailComponent *component) {
	status_t status = B_OK;

	if (_num_components == 0)
		_body = component;
	else if (_num_components == 1) {
		MIMEMultipartContainer *container = new MIMEMultipartContainer(mime_boundary,mime_warning);
		if ((status = container->AddComponent(_body)) == B_OK)
			status = container->AddComponent(component);
		_body = container;
	} else
		status = ((MIMEMultipartContainer *)(_body))->AddComponent(component);

	if (status == B_OK)
		_num_components++;
	return status;
}

status_t MailMessage::RemoveComponent(int32 /*index*/) {
	// not yet implemented
	return B_ERROR;
}


MailComponent *MailMessage::GetComponent(int32 i) {
	if (dynamic_cast<MIMEMultipartContainer *>(_body))
		return ((MIMEMultipartContainer *)(_body))->GetComponent(i);

	if (i < _num_components)
		return _body;

	return NULL;
}

int32 MailMessage::CountComponents() const {
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
		
	MailComponent *component = ((MIMEMultipartContainer *)(_body))->GetComponent(i);
	if (component == NULL)
		return false;
	return component->IsAttachment();
}

void MailMessage::SetBodyTextTo(const char *text) {
	if (_text_body == NULL) {
		_text_body = new PlainTextBodyComponent;
		AddComponent(_text_body);
	}
	
	_text_body->SetText(text);
}	
	

PlainTextBodyComponent *MailMessage::Body()
{
	if (_text_body == NULL)
		_text_body = RetrieveTextBody(_body);

	return _text_body;
}


const char *MailMessage::BodyText() {
	if (Body() == NULL)
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


PlainTextBodyComponent *MailMessage::RetrieveTextBody(MailComponent *component)
{
	PlainTextBodyComponent *body = dynamic_cast<PlainTextBodyComponent *>(component);
	if (body != NULL)
		return body;

	MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(component);
	if (container != NULL) {
		for (int32 i = 0; i < container->CountComponents(); i++) {
			if ((component = container->GetComponent(i)) == NULL)
				continue;

			switch (component->ComponentType())
			{
				case MC_PLAIN_TEXT_BODY:
					return (PlainTextBodyComponent *)container->GetComponent(i);
				case MC_MULTIPART_CONTAINER:
					return RetrieveTextBody(component = container->GetComponent(i));
			}
		}
	}
	return NULL;
}


status_t MailMessage::SetToRFC822(BPositionIO *mail_file, size_t length, bool parse_now) {
	if (BFile *file = dynamic_cast<BFile *>(mail_file))
		file->ReadAttr("MAIL:chain",B_INT32_TYPE,0,&_chain_id,sizeof(_chain_id));
	
	mail_file->Seek(0,SEEK_END);
	length = mail_file->Position();
	mail_file->Seek(0,SEEK_SET);
	
	MailComponent::SetToRFC822(mail_file,length,parse_now);
	
	_body = WhatIsThis();
	
	mail_file->Seek(0,SEEK_SET);
	_body->SetToRFC822(mail_file,length,parse_now);
	
	//------------Move headers that we use to us, everything else to _body
	const char *name;
	for (int32 i = 0; (name = _body->HeaderAt(i)) != NULL; i++) {
		if ((strcasecmp(name,"Subject") != 0)
		  && (strcasecmp(name,"To") != 0)
		  && (strcasecmp(name,"From") != 0)
		  && (strcasecmp(name,"Reply-To") != 0)
		  && (strcasecmp(name,"CC") != 0)) {
			RemoveHeader(name);
		}
	}
	
	_body->RemoveHeader("Subject");
	_body->RemoveHeader("To");
	_body->RemoveHeader("From");
	_body->RemoveHeader("Reply-To");
	_body->RemoveHeader("CC");
	
	_num_components = 1;
	if (MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body))
		_num_components = container->CountComponents();
	
	// what about a better error handling/checking?
	_status = B_OK;	
	return B_OK;
}

inline void TrimWhite(BString &string) {
	int32 i;
	for (i = 0; string.ByteAt(i) != 0; i++) {
		if (!isspace(string.ByteAt(i)))
			break;
	}
	string.Remove(0,i);
	
	for (i = string.Length() - 1; i > 0; i--) {
		if (!isspace(string.ByteAt(i)))
			break;
	}
	string.Truncate(i+1);
}

status_t MailMessage::RenderToRFC822(BPositionIO *file) {
	if (_body == NULL)
		return B_MAIL_INVALID_MAIL;
	
	//------Do real rendering
	
	if (From() == NULL)
		SendViaAccount(_chain_id); //-----Set the from string
	
	BString recipients;
	
	recipients << To();
	if ((CC() != NULL) && (strlen(CC()) > 0))
		recipients << ',' << CC();
	
	if ((_bcc != NULL) && (strlen(_bcc) > 0))
		recipients << ',' << _bcc;
	
	//----Turn "blorp" <blorp@foo.com>,foo@bar.com into <blorp@foo.com>,<foo@bar.com>
	const char *flat = recipients.String();
	StringList recipientsList;
	BString little;
	bool string = false;
	int32 i,j;
	for (i = j = 0; i <= recipients.Length(); i++) {
		if (flat[i] == '"')
		{
			string = !string;
			if (!string)
				j = i+1;
		}
		else if (!string && (flat[i] == ',' || flat[i] == '\0')) {
			little.SetTo(flat + j,i - j);
			puts(little.String());
			
			int32 first,last;
			if ((first = little.FindFirst('(')) >= 0 && (last = little.FindFirst(')')) > 0)
				little.Remove(first,last - first);

			TrimWhite(little);
			recipientsList.AddItem(little.String());
			j = i;
		}
	}
	recipients = "";
	for (i = 0; i < recipientsList.CountItems(); i++) {
		little = recipientsList.ItemAt(i);
		int32 first,last;
		if ((first = little.FindFirst('<')) >= 0 && (last = little.FindFirst('>')) > 0) {
			little.Remove(0,first);
			little.Remove(last + 1,little.Length() - last);
		} else {
			little.Prepend("<");
			little.Append(">");
		}
		
		if (i)
			recipients << ',';
		recipients << little;
	}
	
	if (BFile *attributed = dynamic_cast <BFile *>(file)) {
		attributed->WriteAttrString(B_MAIL_ATTR_RECIPIENTS,&recipients);
		
		BString attr;
		
		attr = To();
		attributed->WriteAttrString(B_MAIL_ATTR_TO,&attr);
		attr = CC();
		attributed->WriteAttrString(B_MAIL_ATTR_CC,&attr);
		attr = Subject();
		attributed->WriteAttrString(B_MAIL_ATTR_SUBJECT,&attr);
		attr = ReplyTo();
		attributed->WriteAttrString(B_MAIL_ATTR_REPLY,&attr);
		attr = From();
		attributed->WriteAttrString(B_MAIL_ATTR_FROM,&attr);
		attr = "Pending";
		attributed->WriteAttrString(B_MAIL_ATTR_STATUS,&attr);
		attr = "1.0";
		attributed->WriteAttrString(B_MAIL_ATTR_MIME,&attr);
		attr = MailChain(_chain_id).Name();
		attributed->WriteAttrString("MAIL:account",&attr);
		
		int32 int_attr;
		
		int_attr = time(NULL);
		attributed->WriteAttr(B_MAIL_ATTR_WHEN,B_TIME_TYPE,0,&int_attr,sizeof(int32));
		int_attr = B_MAIL_PENDING | B_MAIL_SAVE;
		attributed->WriteAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&int_attr,sizeof(int32));
		
		attributed->WriteAttr("MAIL:chain",B_INT32_TYPE,0,&_chain_id,sizeof(int32));
		
		BNodeInfo(attributed).SetType(B_MAIL_TYPE);
	}
	
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
	SetHeaderField("Message-ID", message_id.String());
	
	status_t err = MailComponent::RenderToRFC822(file);
	if (err < B_OK)
		return err;
	
	file->Seek(-2,SEEK_CUR); //-----Remove division between headers
	
	err = _body->RenderToRFC822(file);
	return err;
}
	
status_t MailMessage::RenderTo(BDirectory *dir) {
	BString name = Subject();
	name.ReplaceAll('/','\\');
	name.Prepend("\"");
	name << "\": <" << To() << ">";
	
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
	
	return RenderToRFC822(&file);
}
	
status_t MailMessage::Send(bool send_now) {
	MailChain *via = new MailChain(_chain_id);
	if ((via->InitCheck() != B_OK) || (via->ChainDirection() != outbound)) {
		delete via;
		via = new MailChain(MailSettings().DefaultOutboundChainID());
		SendViaAccount(via->ID());
	}
	
	create_directory(via->MetaData()->FindString("path"),0777);
	BDirectory directory(via->MetaData()->FindString("path"));

	status_t status = RenderTo(&directory);
	if (status >= B_OK && send_now)
		status = MailDaemon::SendQueuedMail();
	
	delete via;
	
	return status;
}


