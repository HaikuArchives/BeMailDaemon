#ifndef ZOIDBERG_NUMAIL_MAILATTACHMENT_H
#define ZOIDBERG_NUMAIL_MAILATTACHMENT_H

#include <Node.h>

#include <MailContainer.h>
#include <MailComponent.h>

class MailAttachment {
	public:
		virtual status_t FileName(char *name) = 0;
		virtual void SetFileName(const char *name) = 0;
};

class SimpleMailAttachment : public MailComponent, public MailAttachment {
	public:
		SimpleMailAttachment();
		
		SimpleMailAttachment(BPositionIO *data /* data to attach */);
		SimpleMailAttachment(const void *data, size_t length /* data to attach */);
		virtual ~SimpleMailAttachment();
		
		virtual status_t FileName(char *name);
		virtual void SetFileName(const char *name);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual BPositionIO *GetDecodedData();
		virtual status_t SetDecodedData(const void *data, size_t length /* data to attach */);
		virtual status_t SetDecodedDataAndDeleteWhenDone(BPositionIO *data);
		
		
		void SetEncoding(mail_encoding encoding = base64 /* note: we only support base64. This is a no-op */);
		mail_encoding Encoding();
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		BPositionIO *_data;
		bool _we_own_data;
		
		mail_encoding _encoding;
};

class AttributedMailAttachment : public MIMEMultipartContainer, public MailAttachment {
	public:
		AttributedMailAttachment(BFile *file, bool delete_when_done);
		AttributedMailAttachment(entry_ref *ref);
		
		AttributedMailAttachment();
		virtual ~AttributedMailAttachment();
		
		void SetTo(BFile *file, bool delete_file_when_done = false);
		void SetTo(entry_ref *ref);
		
		void SaveToDisk(BEntry *entry);
		//-----we pay no attention to entry, but set it to the location of our file in /tmp
		
		void SetEncoding(mail_encoding encoding = base64 /* note: we only support base64. This is a no-op */);
		mail_encoding Encoding();
		
		virtual status_t FileName(char *name);
		virtual void SetFileName(const char *name);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	private:
		SimpleMailAttachment *_data, *_attributes_attach;
		BMessage _attributes;
};

#endif
