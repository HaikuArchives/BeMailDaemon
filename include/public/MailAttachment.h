#ifndef ZOIDBERG_NUMAIL_MAILATTACHMENT_H
#define ZOIDBERG_NUMAIL_MAILATTACHMENT_H

#include <Node.h>

#include <MailContainer.h>
#include <MailComponent.h>

class SimpleMailAttachment : public MailComponent {
	public:
		SimpleMailAttachment();
		
		SimpleMailAttachment(BPositionIO *data /* data to attach */);
		SimpleMailAttachment(const void *data, size_t length /* data to attach */);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t SetDecodedData(const void *data, size_t length /* data to attach */);
		
		void SetEncoding(mail_encoding encoding = base64 /* note: we only support base64. This is a no-op */);
		mail_encoding Encoding();
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		BPositionIO *_data;
		bool _we_own_data;
		
		mail_encoding _encoding;
};

class AttributedMailAttachment : public MIMEMultipartContainer {
	public:
		AttributedMailAttachment(BNode *node);
		AttributedMailAttachment(entry_ref *ref);
		
		void SetTo(BNode *node);
		void SetTo(entry_ref *ref);
		
		void SetEncoding(mail_encoding encoding = base64 /* note: we only support base64. This is a no-op */);
		mail_encoding Encoding();
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t MIMEType(BMimeType *mime);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
		
	private:
		BNode *data;
};

#endif
