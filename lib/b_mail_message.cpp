/* BMailMessage - compatibility wrapper to our mail message class
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


//------This entire document is a horrible, horrible hack. I apologize.
#include <Entry.h>

class _EXPORT BMailMessage;

#include <E-mail.h>

#include <MailAttachment.h>
#include <MailMessage.h>

#include <stdio.h>


namespace Zoidberg {
namespace Mail {

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

extern const CharsetConversionEntry charsets[];

}	// namespace Mail
}	// namespace Zoidberg


BMailMessage::BMailMessage(void)
	:	fFields((BList *)(new Zoidberg::Mail::Message()))
{
}

BMailMessage::~BMailMessage(void)
{
	delete ((Zoidberg::Mail::Message *)(fFields));
}

status_t BMailMessage::AddContent(const char *text, int32 length,
	uint32 encoding, bool /*clobber*/)
{
	Zoidberg::Mail::TextComponent *comp = new Zoidberg::Mail::TextComponent;
	BMemoryIO io(text,length);
	comp->SetDecodedData(&io);
	
	comp->SetEncoding(quoted_printable,encoding);
	
	//if (clobber)
	((Zoidberg::Mail::Message *)(fFields))->AddComponent(comp);
	
	return B_OK;
}
					
status_t BMailMessage::AddContent(const char *text, int32 length,
	const char *encoding, bool /*clobber*/)
{
	Zoidberg::Mail::TextComponent *comp = new Zoidberg::Mail::TextComponent();
	BMemoryIO io(text,length);
	comp->SetDecodedData(&io);
	
	uint32 encode = B_ISO1_CONVERSION;
	//-----I'm assuming that encoding is one of the RFC charsets
	//-----there are no docs. Am I right?
	if (encoding != NULL) {
		for (int32 i = 0; Zoidberg::Mail::charsets[i].charset != NULL; i++) {
			if (strcasecmp(encoding,Zoidberg::Mail::charsets[i].charset) == 0) {
				encode = Zoidberg::Mail::charsets[i].flavor;
				break;
			}
		}
	}
	
	comp->SetEncoding(quoted_printable,encode);
	
	//if (clobber)
	((Zoidberg::Mail::Message *)(fFields))->AddComponent(comp);
	
	return B_OK;
}

status_t BMailMessage::AddEnclosure(entry_ref *ref, bool /*clobber*/)
{
	((Zoidberg::Mail::Message *)(fFields))->Attach(ref);
	return B_OK;
}

status_t BMailMessage::AddEnclosure(const char *path, bool /*clobber*/)
{
	BEntry entry(path);
	status_t status;
	if ((status = entry.InitCheck()) < B_OK)
		return status;
	
	entry_ref ref;
	if ((status = entry.GetRef(&ref)) < B_OK)
		return status;

	((Zoidberg::Mail::Message *)(fFields))->Attach(&ref);
	return B_OK;
}

status_t BMailMessage::AddEnclosure(const char *MIME_type, void *data, int32 len,
	bool /*clobber*/)
{
	Zoidberg::Mail::SimpleAttachment *attach = new Zoidberg::Mail::SimpleAttachment;
	attach->SetDecodedData(data,len);
	attach->SetHeaderField("Content-Type",MIME_type);
	
	((Zoidberg::Mail::Message *)(fFields))->AddComponent(attach);
	return B_OK;
}

status_t BMailMessage::AddHeaderField(uint32 /*encoding*/, const char *field_name, const char *str, 
	bool /*clobber*/)
{
	//printf("First AddHeaderField. Args are %s%s\n",field_name,str);
	
	BString string = field_name;
	string.Truncate(string.Length() - 2); //----BMailMessage includes the ": "
	((Zoidberg::Mail::Message *)(fFields))->SetHeaderField(string.String(),str);
	return B_OK;
}

status_t BMailMessage::AddHeaderField(const char *field_name, const char *str,
	bool /*clobber*/)
{
	//printf("Second AddHeaderField. Args are %s%s\n",field_name,str);
	BString string = field_name;
	string.Truncate(string.Length() - 2); //----BMailMessage includes the ": "
	((Zoidberg::Mail::Message *)(fFields))->SetHeaderField(string.String(),str);
	return B_OK;
}

status_t BMailMessage::Send(bool send_now,
	 bool /*remove_when_I_have_completed_sending_this_message_to_your_preferred_SMTP_server*/)
{
 	return ((Zoidberg::Mail::Message *)(fFields))->Send(send_now);
}

