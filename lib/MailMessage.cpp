/* Message - the main general purpose mail message class
**
** Copyright 2001-2002 Dr. Zoidberg Enterprises. All rights reserved.
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
#include <stdlib.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <parsedate.h>

#ifdef BONE
	#ifdef _KERNEL_MODE
		#undef _KERNEL_MODE
		#include <sys/socket.h>
		#define _KERNEL_MODE 1
	#endif
	#include <bone_serial_ppp.h>
	#include <unistd.h>
#endif

namespace Zoidberg {
namespace Mail {
	class _EXPORT Message;
}
}

#include <MailMessage.h>
#include <MailAttachment.h>
#include <MailSettings.h>
#include <MailDaemon.h>
#include <mail_util.h>
#include <StringList.h>

using namespace Zoidberg;
using namespace Zoidberg::Mail;

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."


Message::Message(BPositionIO *file, bool own, uint32 defaultCharSet)
	:
	Container (defaultCharSet),
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


Message::Message(entry_ref *ref, uint32 defaultCharSet)
	:
	Container (defaultCharSet),
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


Message *
Message::ReplyMessage(reply_to_mode replyTo, bool accountFromMail, const char *quoteStyle)
{
	Mail::Message *to_return = new Mail::Message;

	// Set ReplyTo:

	if (replyTo == MD_REPLY_TO_ALL) {
		to_return->SetTo(From());

		BList list;
		get_address_list(list, CC(), extract_address);
		get_address_list(list, To(), extract_address);

		// Filter out the sender
		BString sender = Mail::Chain(Account()).MetaData()->FindString("reply_to");
		extract_address(sender);

		BString cc;

		for (int32 i = list.CountItems(); i-- > 0;) {
			char *address = (char *)list.RemoveItem(0L);

			// add everything which is not the sender and not already in the list
			if (sender.ICompare(address) && cc.FindFirst(address) < 0) {
				if (cc.Length() > 0)
					cc << ", ";

				cc << address;
			}

			free(address);
		}

		if (cc.Length() > 0)
			to_return->SetCC(cc.String());
	} else if (replyTo == MD_REPLY_TO_SENDER || ReplyTo() == NULL)
		to_return->SetTo(From());
	else
		to_return->SetTo(ReplyTo());

	// Set special "In-Reply-To:" header (used for threading)
	const char *messageID = _body ? _body->HeaderField("Message-Id") : NULL;
	if (messageID != NULL)
		to_return->SetHeaderField("In-Reply-To", messageID);

	// quote body text
	to_return->SetBodyTextTo(BodyText());
	if (quoteStyle)
		to_return->Body()->Quote(quoteStyle);

	// Set the subject (and add a "Re:" if needed)
	BString string = Subject();
	if (string.ICompare("re:", 3) != 0)
		string.Prepend("Re: ");
	to_return->SetSubject(string.String());

	// set the matching outbound chain
	if (accountFromMail)
		to_return->SendViaAccountFrom(this);

	return to_return;
}


Message *
Message::ForwardMessage(bool accountFromMail, bool includeAttachments)
{
	BString header = "------ Forwarded Message: ------\n";
	header << "To: " << To() << '\n';
	header << "From: " << From() << '\n';
	if (CC() != NULL)
		header << "CC: " << CC() << '\n'; // Can use CC rather than "Cc" since display only.
	header << "Subject: " << Subject() << '\n';
	header << "Date: " << Date() << "\n\n";
	header << _text_body->Text() << '\n';
	Mail::Message *message = new Mail::Message();
	message->SetBodyTextTo(header.String());

	// set the subject
	BString subject = Subject();
	if (subject.IFindFirst("fwd") == B_ERROR
		&& subject.IFindFirst("forward") == B_ERROR
		&& subject.FindFirst("FW") == B_ERROR)
		subject << " (fwd)";
	message->SetSubject(subject.String());

	if (includeAttachments) {
		for (int32 i = 0; i < CountComponents(); i++) {
			Mail::Component *cmpt = GetComponent(i);
			if (cmpt == _text_body)
				continue;

			//---I am ashamed to have the written the code between here and the next comment
			cmpt->GetDecodedData(NULL);
			// ... and you still managed to get it wrong ;-)), axeld.
			// we should really move this stuff into copy constructors
			// or something like that

			BMallocIO io;
			cmpt->RenderToRFC822(&io);
			Mail::Component *clone = cmpt->WhatIsThis();
			io.Seek(0, SEEK_SET);
			clone->SetToRFC822(&io, io.BufferLength(), true);
			message->AddComponent(clone);
		//---
		}
	}

	if (accountFromMail)
		message->SendViaAccountFrom(this);

	return message;
}


const char *
Message::To()
{
	return HeaderField("To");
}


const char *
Message::From()
{
	return HeaderField("From");
}


const char *
Message::ReplyTo()
{
	return HeaderField("Reply-To");
}


const char *
Message::CC()
{
	return HeaderField("Cc"); // Note case of CC is "Cc" in our internal headers.
}


const char *
Message::Subject()
{
	return HeaderField("Subject");
}


const char *
Message::Date()
{
	return HeaderField("Date");
}


void Message::SetSubject(const char *subject, uint32 charset, mail_encoding encoding) {
	SetHeaderField("Subject", subject, charset, encoding);
}

void Message::SetReplyTo(const char *reply_to, uint32 charset, mail_encoding encoding) {
	SetHeaderField("Reply-To", reply_to, charset, encoding);
}

void Message::SetFrom(const char *from, uint32 charset, mail_encoding encoding) {
	SetHeaderField("From", from, charset, encoding);
}

void Message::SetTo(const char *to, uint32 charset, mail_encoding encoding) {
	SetHeaderField("To", to, charset, encoding);
}

void Message::SetCC(const char *cc, uint32 charset, mail_encoding encoding) {
	// For consistency with our header names, use Cc as the name.
	SetHeaderField("Cc", cc, charset, encoding);
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
	// ToDo: look at From header?  But usually there is
	// a file since only the BeMail GUI calls this.
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
Message::SendViaAccountFrom(Mail::Message *message)
{
	char name[B_FILE_NAME_LENGTH];
	if (message->GetAccountName(name, B_FILE_NAME_LENGTH) < B_OK) {
		// just return the message with the default account
		return;
	}

	BList chains;
	Mail::OutboundChains(&chains);
	for (int32 i = chains.CountItems();i-- > 0;) {
		Chain *chain = (Chain *)chains.ItemAt(i);
		if (!strcmp(chain->Name(), name))
			SendViaAccount(chain->ID());

		delete chain;
	}
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
		delete (Mail::Chain *)chains.RemoveItem(0L);
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
		MIMEMultipartContainer *container = new MIMEMultipartContainer (
			mime_boundary, mime_warning, _charSetForTextDecoding);
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


status_t
Message::RemoveComponent(Mail::Component */*component*/)
{
	// not yet implemented
	// BeMail/Enclosures.cpp:169: contains a warning about this fact
	return B_ERROR;
}


status_t
Message::RemoveComponent(int32 /*index*/)
{
	// not yet implemented
	return B_ERROR;
}


Mail::Component *
Message::GetComponent(int32 i, bool parse_now)
{
	if (MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body))
		return container->GetComponent(i, parse_now);

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
					body = dynamic_cast<Mail::TextComponent *>(container->GetComponent(i));
					if (body != NULL)
						return body;
					break;

				case MC_MULTIPART_CONTAINER:
					body = RetrieveTextBody(container->GetComponent(i));
					if (body != NULL)
						return body;
					break;
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
		  && (strcasecmp(name,"Cc") != 0)
		  && (strcasecmp(name,"Date") != 0)) {
			RemoveHeader(name);
		}
	}

	_body->RemoveHeader("Subject");
	_body->RemoveHeader("To");
	_body->RemoveHeader("From");
	_body->RemoveHeader("Reply-To");
	_body->RemoveHeader("Cc");
	_body->RemoveHeader("Date");

	_num_components = 1;
	if (MIMEMultipartContainer *container = dynamic_cast<MIMEMultipartContainer *>(_body))
		_num_components = container->CountComponents();

	return B_OK;
}


status_t
Message::RenderToRFC822(BPositionIO *file)
{
	if (_body == NULL)
		return B_MAIL_INVALID_MAIL;

	//------Do real rendering

	if (From() == NULL)
		SendViaAccount(_chain_id); //-----Set the from string

	BList recipientList;
	get_address_list(recipientList, To(), extract_address);
	get_address_list(recipientList, CC(), extract_address);
	get_address_list(recipientList, _bcc, extract_address);

	BString recipients;
	for (int32 i = recipientList.CountItems(); i-- > 0;) {
		char *address = (char *)recipientList.RemoveItem(0L);

		recipients << '<' << address << '>';
		if (i)
			recipients << ',';

		free(address);
	}

	// add the date field
	int32 creationTime = time(NULL);
	{
		char date[128];
		struct tm tm;
		localtime_r(&creationTime, &tm);

		strftime(date, 128, "%a, %d %b %Y %H:%M:%S",&tm);

		// GMT offsets are full hours, yes, but you never know :-)
		if (tm.tm_gmtoff)
			sprintf(date + strlen(date)," %+03d%02d",tm.tm_gmtoff / 3600,(tm.tm_gmtoff / 60) % 60);

		uint32 length = strlen(date);
		if (length < sizeof(date) - 5)
			strftime(date + length, length - sizeof(date), " %Z", &tm);

		SetHeaderField("Date", date);
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

	file->Seek(-2, SEEK_CUR); //-----Remove division between headers

	err = _body->RenderToRFC822(file);
	if (err < B_OK)
		return err;

	// Set the message file's attributes.  Do this after the rest of the file
	// is filled in, in case the daemon attempts to send it before it is ready
	// (since the daemon may send it when it sees the status attribute getting
	// set to "Pending").

	if (BFile *attributed = dynamic_cast <BFile *>(file)) {

		BNodeInfo(attributed).SetType(B_MAIL_TYPE);

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
	}

	return B_OK;
}


status_t
Message::RenderTo(BDirectory *dir)
{
	time_t		currentTime;
	char		numericDateString [40];
	struct tm   timeFields;
	BString		worker;

	// Generate a file name for the outgoing message.  See also
	// FolderFilter::ProcessMailMessage which does something similar for
	// incoming messages.

	BString name = Subject();
	SubjectToThread (name); // Extract the core subject words.
	if (name.Length() <= 0)
		name = "No Subject";
	if (name[0] == '.')
		name.Prepend ("_"); // Avoid hidden files, starting with a dot.

	// Convert the date into a year-month-day fixed digit width format, so that
	// sorting by file name will give all the messages with the same subject in
	// order of date.
	time (&currentTime);
	localtime_r (&currentTime, &timeFields);
	sprintf (numericDateString, "%04d%02d%02d%02d%02d%02d",
		timeFields.tm_year + 1900,
		timeFields.tm_mon + 1,
		timeFields.tm_mday,
		timeFields.tm_hour,
		timeFields.tm_min,
		timeFields.tm_sec);
	name << " " << numericDateString;

	worker = From();
	extract_address_name(worker);
	name << " " << worker;

	name.Truncate(222);	// reserve space for the uniquer

	// Get rid of annoying characters which are hard to use in the shell.
	name.ReplaceAll('/','_');
	name.ReplaceAll('\'','_');
	name.ReplaceAll('"','_');

	int32 uniquer = time(NULL);
	worker = name;

	int32 tries = 30;
	bool exists;
	while (((exists = dir->Contains(worker.String())) == true) /* pacify mwcc */ &&
		(--tries > 0)) {
		srand(rand());
		uniquer += (rand() >> 16) - 16384;

		worker = name;
		worker << ' ' << uniquer;
	}

	if (exists)
		printf("could not create mail! (should be: %s)\n", worker.String());

	BFile file;
	status_t status = dir->CreateFile(worker.String(), &file);
	if (status < B_OK)
		return status;

	return RenderToRFC822(&file);
}


status_t
Message::Send(bool send_now)
{
	Mail::Chain *via = new Mail::Chain(_chain_id);
	if ((via->InitCheck() != B_OK) || (via->ChainDirection() != outbound)) {
		delete via;
		via = new Mail::Chain(Mail::Settings().DefaultOutboundChainID());
		SendViaAccount(via->ID());
	}

	create_directory(via->MetaData()->FindString("path"),0777);
	BDirectory directory(via->MetaData()->FindString("path"));

	status_t status = RenderTo(&directory);
	delete via;
	if (status >= B_OK && send_now) {
		Mail::Settings settings_file;
		if (settings_file.SendOnlyIfPPPUp()) {
#ifdef BONE
			int s = socket(AF_INET, SOCK_DGRAM, 0);
			bsppp_status_t ppp_status;

			strcpy(ppp_status.if_name, "ppp0");
			if (ioctl(s, BONE_SERIAL_PPP_GET_STATUS, &ppp_status, sizeof(ppp_status)) != 0) {
				close(s);
				return B_OK;
			} else {
				if (ppp_status.connection_status != BSPPP_CONNECTED) {
					close(s);
					return B_OK;
				}
			}
			close(s);
#else
			if (find_thread("tty_thread") <= 0)
				return B_OK;
#endif
			status = Mail::SendQueuedMail();
		} else {
			status = Mail::SendQueuedMail();
		}
	}

	return status;
}

void Message::_ReservedMessage1() {}
void Message::_ReservedMessage2() {}
void Message::_ReservedMessage3() {}

