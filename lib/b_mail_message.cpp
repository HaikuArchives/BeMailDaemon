//------This entire document is a horrible, horrible hack. I apologize.

#include <base64.h>

#include <E-mail.h>
#include <Entry.h>

class _EXPORT BMailMessage;

#include <MailAttachment.h>
#include <MailMessage.h>

#if __POWERPC__
#define encode_base64__local_abi encode_base64__FPcPcx
#define decode_base64__local_abi decode_base64__FPcPcxb
#else
#error "Find out what the GCC 2 ABI is and fix this!"
#endif

extern "C" _EXPORT ssize_t encode_base64__local_abi(char *out, char *in, off_t length);
extern "C" _EXPORT ssize_t decode_base64__local_abi(char *out, char *in, off_t length, bool replace_cr);

ssize_t encode_base64__local_abi(char *out, char *in, off_t length) {
	return encode_base64(out,in,length);
}

ssize_t decode_base64__local_abi(char *out, char *in, off_t length, bool replace_cr) {
	return decode_base64(out,in,length);
}

BMailMessage::BMailMessage(void) : fFields((BList *)(new MailMessage())) {}
BMailMessage::~BMailMessage(void) {
	delete (MailMessage *)(fFields);
}

status_t	BMailMessage::AddContent(const char *text, int32 length,
		   uint32 encoding,
		   bool clobber) {
				PlainTextBodyComponent *comp = new PlainTextBodyComponent;
				BMemoryIO io(text,length);
				comp->SetDecodedData(&io);
				
				comp->SetEncoding(quoted_printable,encoding);
				
				//if (clobber)
				((MailMessage *)(fFields))->AddComponent(comp);
				
				return B_OK;
			}
					
/*status_t	BMailMessage::AddContent(const char *text, int32 length,
		   const char *encoding, bool clobber = false) {*/
		   		

status_t	BMailMessage::AddEnclosure(entry_ref *ref, bool clobber) {
		((MailMessage *)(fFields))->Attach(ref);
		return B_OK;
}

status_t	BMailMessage::AddEnclosure(const char *path, bool clobber) {
	entry_ref ref;
	BEntry(path).GetRef(&ref);
	((MailMessage *)(fFields))->Attach(&ref);
}

status_t	BMailMessage::AddEnclosure(const char *MIME_type, void *data, int32 len,
			 bool clobber) {
			 	SimpleMailAttachment *attach = new SimpleMailAttachment;
			 	attach->SetDecodedData(data,len);
			 	attach->AddHeaderField("Content-Type",MIME_type);
			 	
			 	((MailMessage *)(fFields))->AddComponent(attach);
}

status_t	BMailMessage::AddHeaderField(uint32 encoding, const char *field_name, const char *str, 
			   bool clobber) {
					((MailMessage *)(fFields))->SetHeaderField(field_name,str);
				}
				
status_t	BMailMessage::AddHeaderField(const char *field_name, const char *str,
			   bool clobber) {
					((MailMessage *)(fFields))->SetHeaderField(field_name,str);
				}

status_t	BMailMessage::Send(bool send_now,
	 bool remove_when_I_have_completed_sending_this_message_to_your_preferred_SMTP_server) {
	 	return ((MailMessage *)(fFields))->Send(send_now);
	 }