#include <List.h>
#include <String.h>

#include <malloc.h>
#include <string.h>

class _EXPORT MailMessage;

#include <MailMessage.h>
#include <MailSettings.h>

//-------Change the following!----------------------
#define mime_boundary "----------Zoidberg-BeMail-temp--------"
#define mime_warning "This is a multipart message in MIME format."

MailMessage::MailMessage(BPositionIO *mail_file) : _num_components(0), _body(NULL) {}

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
			_chain_id = ((MailChain *)(chains.ItemAt(i)))->ID();
			break;
		}
	}
	
	while (chains.CountItems() > 0)
		delete chains.RemoveItem(0L);
}
	
void MailMessage::SendViaAccount(int32 chain_id) {
	_chain_id = chain_id;
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

//----Come back here
//void MailMessage::Send(bool send_now, bool delete_on_send);