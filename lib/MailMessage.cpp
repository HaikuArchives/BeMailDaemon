/* Message - the main general purpose mail message class
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


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

namespace Zoidberg {
namespace Mail {
	class _EXPORT Message;
}
}

#include <MailMessage.h>
#include <MailAttachment.h>
#include <MailSettings.h>
#include <MailDaemon.h>
#include <StringList.h>

using namespace Zoidberg;
using namespace Zoidberg::Mail;

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."


Message::Message(BPositionIO *file,bool own)
	:
	fData(NULL),
	_status(B_NO_ERROR),
	_bcc(NULL),
	_num_components(0),
	_body(NULL),
	_text_body(NULL)
{
	Mail::Settings settings;
	_chain_id = settings.DefaultOutboundChainID();

	if (own)
		fData = file;

	if (file != NULL)
		SetToRFC822(file,-1);
}


Message::Message(entry_ref *ref)
	:
	_bcc(NULL),
	_num_components(0),
	_body(NULL),
	_text_body(NULL)
{
	Mail::Settings settings;
	_chain_id = settings.DefaultOutboundChainID();

	fData = new BFile();
	_status = static_cast<BFile *>(fData)->SetTo(ref,B_READ_ONLY);

	if (_status == B_OK)
		SetToRFC822(fData,-1);
}


Message::~Message()
{
	if (_bcc != NULL)
		free(_bcc);
	
	delete _body;
	delete fData;
}


status_t Message::InitCheck() const
{
	return _status;
}

const char *Message::To() {
	return HeaderField("To");
}

const char *Message::From() {
	return HeaderField("From");
}

const char *Message::ReplyTo() {
	return HeaderField("Reply-To");
}

const char *Message::CC() {
	return HeaderField("CC");
}

const char *Message::Subject() {
	return HeaderField("Subject");
}


const char *
Message::Date()
{
	return HeaderField("Date");
}


void Message::SetSubject(const char *subject) {
	SetHeaderField("Subject",subject);
}

void Message::SetReplyTo(const char *reply_to) {
	SetHeaderField("Reply-To",reply_to);
}

void Message::SetFrom(const char *from) {
	SetHeaderField("From",from);
}

void Message::SetTo(const char *to) {
	SetHeaderField("To",to);
}

void Message::SetCC(const char *cc) {
	SetHeaderField("CC",cc);
}

void Message::SetBCC(const char *bcc) {
	if (_bcc != NULL)
		free(_bcc);
		
	_bcc = strdup(bcc);
}


status_t 
Message::GetName(char *name, int32 maxLength) const
{
	if (name == NULL || maxLength <= 0)
		return B_BAD_VALUE;

	if (BFile *file = dynamic_cast<BFile *>(fData)) {
		status_t status = file->ReadAttr(B_MAIL_ATTR_NAME,B_STRING_TYPE,0,name,maxLength);
		name[maxLength - 1] = '\0';

		return status >= 0 ? B_OK : status;
	}

	return B_ERROR;
}


status_t 
Message::GetName(BString *name) const
{
	char *buffer = name->LockBuffer(B_FILE_NAME_LENGTH);
	status_t status = GetName(buffer,B_FILE_NAME_LENGTH);
	name->UnlockBuffer();

	return status;
}


void
Message::SendViaAccount(const char *account_name)
{
	BList chains;
	Mail::OutboundChains(&chains);
	
	for (int32 i = 0; i < chains.CountItems(); i++) {
		if (strcmp(((Mail::Chain *)(chains.ItemAt(i)))->Name(),account_name) == 0) {
			SendViaAccount(((Mail::Chain *)(chains.ItemAt(i)))->ID());
			break;
		}
	}
	
	while (chains.CountItems() > 0)
		delete chains.RemoveItem(0L);
}

	
void
Message::SendViaAccount(int32 chain_id)
{
	_chain_id = chain_id;
	
	Mail::Chain chain(_chain_id);
	BString from;
	from << '\"' << chain.MetaData()->FindString("real_name") << "\" <" << chain.MetaData()->FindString("reply_to") << '>';
	SetFrom(from.String());
}


int32
Message::Account() const
{
	return _chain_id;
}


status_t 
Message::GetAccountName(char *account,int32 maxLength) const
{
	if (account == NULL || maxLength <= 0)
		return B_BAD_VALUE;

	if (BFile *file = dynamic_cast<BFile *>(fData)) {
		status_t status = file->ReadAttr(B_MAIL_ATTR_ACCOUNT,B_STRING_TYPE,0,account,maxLength);
		account[maxLength - 1] = '\0';

		return status >= 0 ? B_OK : status;
	}

	// ToDo: try to get account name out of the chain lists
	return B_ERROR;
}


status_t 
Message::GetAccountName(BString *account) const
{
	char *buffer = account->LockBuffer(B_FILE_NAME_LENGTH);
	status_t status = GetAccountName(buffer,B_FILE_NAME_LENGTH);
	account->UnlockBuffer();

	return status;
}


status_t
Message::AddComponent(Mail::Component *component)
{
	status_t status = B_OK;

	if (_num_components == 0)
		_body = component;
	else if (_num_components == 1) {
		MIMEMultipartContainer *container = new MIMEMultipartContainer(mime_boundary,mime_warning);
		if ((status = container->AddComponent(_body)) == B_OK)
			status = container->AddComponent(component);
		_body = container;
	} else {
		MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body);
		if (container == NULL)
			return B_MISMATCHED_VALUES; //---This really needs a B_WTF constant...
		
		status = container->AddComponent(component);
	}

	if (status == B_OK)
		_num_components++;
	return status;
}

status_t Message::RemoveComponent(int32 /*index*/) {
	// not yet implemented
	return B_ERROR;
}


Mail::Component *Message::GetComponent(int32 i) {
	if (MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body))
		return container->GetComponent(i);

	if (i < _num_components)
		return _body;

	return NULL;
}

int32 Message::CountComponents() const {
	return _num_components;
}

void Message::Attach(entry_ref *ref, bool includeAttributes)
{
	if (includeAttributes)
		AddComponent(new AttributedAttachment(ref));
	else
		AddComponent(new SimpleAttachment(ref));
}
		
bool Message::IsComponentAttachment(int32 i) {
	if ((i >= _num_components) || (_num_components == 0))
		return false;
		
	if (_num_components == 1)
		return _body->IsAttachment();
	
	MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body);
	if (container == NULL)
		return false; //-----This should never, ever, ever, ever, happen
	
	Mail::Component *component = container->GetComponent(i);
	if (component == NULL)
		return false;
	return component->IsAttachment();
}

void Message::SetBodyTextTo(const char *text) {
	if (_text_body == NULL) {
		_text_body = new Mail::TextComponent;
		AddComponent(_text_body);
	}
	
	_text_body->SetText(text);
}	
	

Mail::TextComponent *Message::Body()
{
	if (_text_body == NULL)
		_text_body = RetrieveTextBody(_body);

	return _text_body;
}


const char *Message::BodyText() {
	if (Body() == NULL)
		return NULL;

	return _text_body->Text();
}


status_t Message::SetBody(Mail::TextComponent *body) {
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


Mail::TextComponent *Message::RetrieveTextBody(Mail::Component *component)
{
	Mail::TextComponent *body = dynamic_cast<Mail::TextComponent *>(component);
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
					// AttributedAttachment returns the MIME type of its contents, so
					// we have to use dynamic_cast here
					return dynamic_cast<Mail::TextComponent *>(container->GetComponent(i));
				case MC_MULTIPART_CONTAINER:
					return RetrieveTextBody(component = container->GetComponent(i));
			}
		}
	}
	return NULL;
}


status_t
Message::SetToRFC822(BPositionIO *mail_file, size_t length, bool parse_now)
{
	if (BFile *file = dynamic_cast<BFile *>(mail_file))
		file->ReadAttr("MAIL:chain",B_INT32_TYPE,0,&_chain_id,sizeof(_chain_id));

	mail_file->Seek(0,SEEK_END);
	length = mail_file->Position();
	mail_file->Seek(0,SEEK_SET);

	_status = Mail::Component::SetToRFC822(mail_file,length,parse_now);
	if (_status < B_OK)
		return _status;

	_body = WhatIsThis();

	mail_file->Seek(0,SEEK_SET);
	_status = _body->SetToRFC822(mail_file,length,parse_now);
	if (_status < B_OK)
		return _status;

	//------------Move headers that we use to us, everything else to _body
	const char *name;
	for (int32 i = 0; (name = _body->HeaderAt(i)) != NULL; i++) {
		if ((strcasecmp(name,"Subject") != 0)
		  && (strcasecmp(name,"To") != 0)
		  && (strcasecmp(name,"From") != 0)
		  && (strcasecmp(name,"Reply-To") != 0)
		  && (strcasecmp(name,"CC") != 0)
		  && (strcasecmp(name,"Date") != 0)) {
			RemoveHeader(name);
		}
	}

	_body->RemoveHeader("Subject");
	_body->RemoveHeader("To");
	_body->RemoveHeader("From");
	_body->RemoveHeader("Reply-To");
	_body->RemoveHeader("CC");
	_body->RemoveHeader("Date");

	_num_components = 1;
	if (MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body))
		_num_components = container->CountComponents();

	return B_OK;
}


inline void
TrimWhite(BString &string)
{
	char *buffer = string.LockBuffer(string.Length() + 1);

	int32 i,length = strlen(buffer);
	while (length > 0 && isspace(buffer[length - 1]))
		length--;
	buffer[length] = '\0';

	for (i = 0; buffer[i] && isspace(buffer[i]); i++) {}
	if (i != 0) {
		length -= i;
		memmove(buffer,buffer + i,length + 1);
	}
	string.UnlockBuffer(length);
}


status_t
Message::RenderToRFC822(BPositionIO *file)
{
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
			
			int32 first,last;
			if ((first = little.FindFirst('(')) >= 0 && (last = little.FindFirst(')')) > 0)
				little.Remove(first,last + 1 - first);

			TrimWhite(little);
			recipientsList.AddItem(little.String());

			j = i + 1;
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

	// add the date field	
	int32 creationTime = time(NULL);
	{
		char date[128];
		struct tm tm;
		localtime_r(&creationTime,&tm);

		strftime(date,128,"%a, %d %b %Y %H:%M:%S %Z",&tm);

		// GMT offsets are full hours, yes, but you never know :-)
		if (tm.tm_gmtoff)
			sprintf(date + strlen(date)," (%+03d%02d)",tm.tm_gmtoff / 3600,(tm.tm_gmtoff / 60) % 60);

		SetHeaderField("Date", date);
	}

	// setting the file's attributes

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
		attr = Mail::Chain(_chain_id).Name();
		attributed->WriteAttrString(B_MAIL_ATTR_ACCOUNT,&attr);

		attributed->WriteAttr(B_MAIL_ATTR_WHEN,B_TIME_TYPE,0,&creationTime,sizeof(int32));
		int32 flags = B_MAIL_PENDING | B_MAIL_SAVE;
		attributed->WriteAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&flags,sizeof(int32));

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
	
	status_t err = Mail::Component::RenderToRFC822(file);
	if (err < B_OK)
		return err;
	
	file->Seek(-2,SEEK_CUR); //-----Remove division between headers
	
	err = _body->RenderToRFC822(file);
	return err;
}
	
status_t Message::RenderTo(BDirectory *dir) {
	BString name = Subject();
	name.Prepend("\"");
	name << "\": <" << To();
	name.Truncate(222);	// reserve space for the uniquer
	name << ">";
	name.ReplaceAll('/','\\');

	BString worker;
	int32 uniquer = time(NULL);
	worker = name;
	
	while (dir->Contains(worker.String())) {
		worker = name;
		uniquer++;
		
		worker << ' ' << uniquer;
	}

	BFile file;
	status_t status = dir->CreateFile(worker.String(),&file);
	if (status < B_OK)
		return status;

	return RenderToRFC822(&file);
}
	
status_t Message::Send(bool send_now) {
	Mail::Chain *via = new Mail::Chain(_chain_id);
	if ((via->InitCheck() != B_OK) || (via->ChainDirection() != outbound)) {
		delete via;
		via = new Mail::Chain(Mail::Settings().DefaultOutboundChainID());
		SendViaAccount(via->ID());
	}
	
	create_directory(via->MetaData()->FindString("path"),0777);
	BDirectory directory(via->MetaData()->FindString("path"));

	status_t status = RenderTo(&directory);
	if (status >= B_OK && send_now)
		status = Mail::SendQueuedMail();
	
	delete via;
	
	return status;
}

void Message::_ReservedMessage1() {}
void Message::_ReservedMessage2() {}
void Message::_ReservedMessage3() {}

