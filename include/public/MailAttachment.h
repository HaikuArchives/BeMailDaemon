#ifndef ZOIDBERG_MAIL_ATTACHMENT_H
#define ZOIDBERG_MAIL_ATTACHMENT_H
/* Attachment - classes which handle mail attachments
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Node.h>

#include <MailContainer.h>
#include <MailComponent.h>


namespace Zoidberg {
namespace Mail {

class Attachment {
	public:
		virtual void SetFileName(const char *name) = 0;
		virtual status_t FileName(char *name) = 0;
	
	private:
		virtual void _ReservedAttachment1();
		virtual void _ReservedAttachment2();
		virtual void _ReservedAttachment3();
		virtual void _ReservedAttachment4();
		virtual void _ReservedAttachment5();
};

class SimpleAttachment : public Component, public Attachment {
	public:
		SimpleAttachment();
		
		SimpleAttachment(BPositionIO *data /* data to attach */, mail_encoding encoding = base64);
		SimpleAttachment(const void *data, size_t length /* data to attach */, mail_encoding encoding = base64);
		virtual ~SimpleAttachment();
		
		virtual void SetFileName(const char *name);
		virtual status_t FileName(char *name);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual BPositionIO *GetDecodedData();
		virtual status_t SetDecodedData(const void *data, size_t length /* data to attach */);
		virtual status_t SetDecodedDataAndDeleteWhenDone(BPositionIO *data);
		
		
		void SetEncoding(mail_encoding encoding = base64 /* note: we only support base64. This is a no-op */);
		mail_encoding Encoding();
		
		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);

	private:
		void ParseNow();
		
		virtual void _ReservedSimple1();
		virtual void _ReservedSimple2();
		virtual void _ReservedSimple3();

		BPositionIO *_data, *_raw_data;
		size_t _raw_length;
		off_t _raw_offset;
		bool _we_own_data;
		mail_encoding _encoding;
		
		uint32 _reserved[5];
};

class AttributedAttachment : public MIMEMultipartContainer, public Attachment {
	public:
		AttributedAttachment(BFile *file, bool delete_when_done);
		AttributedAttachment(entry_ref *ref);
		
		AttributedAttachment();
		virtual ~AttributedAttachment();
		
		void SetTo(BFile *file, bool delete_file_when_done = false);
		void SetTo(entry_ref *ref);
		
		void SaveToDisk(BEntry *entry);
		//-----we pay no attention to entry, but set it to the location of our file in /tmp
		
		void SetEncoding(mail_encoding encoding /* anything but uuencode */);
		mail_encoding Encoding();
		
		virtual status_t FileName(char *name);
		virtual void SetFileName(const char *name);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);

	private:
		virtual void _ReservedAttributed1();
		virtual void _ReservedAttributed2();
		virtual void _ReservedAttributed3();

		SimpleAttachment *_data, *_attributes_attach;
		BMessage _attributes;

		uint32 _reserved[5];
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_ATTACHMENT_H */
