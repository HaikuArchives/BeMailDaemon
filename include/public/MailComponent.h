#ifndef ZOIDBERG_NUMAIL_MAILCOMPONENT_H
#define ZOIDBERG_NUMAIL_MAILCOMPONENT_H

#include <UTF8.h>
#include <Message.h>

class BMimeType;
class MailComponent;

typedef enum {
	base64 				= 'b',
	quoted_printable 	= 'q',
	
	no_encoding			= -1
} mail_encoding;

class MailComponent {
	public:
		MailComponent();
		virtual ~MailComponent();
		
		//------Info on this component
		MailComponent *WhatIsThis();
			// Takes any generic MailComponent, and returns an instance
			// of a MailComponent subclass that applies to this case,
			// ready for instantiation. Note that you still have to
			// Instantiate() it yourself.
		bool IsAttachment();
			// Returns true if this component is an attachment, false
			// otherwise
			
		void AddHeaderField(
			const char *key, const char *value,
			uint32 charset = B_ISO1_CONVERSION,
			mail_encoding encoding = quoted_printable,
			bool replace_existing = true);
		const char *HeaderField(const char *key, int32 index = 0);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	
	private:
		BMessage headers;
};

class PlainTextBodyComponent : public MailComponent {
	public:
		PlainTextBodyComponent(const char *text = NULL);
		virtual ~PlainTextBodyComponent();		

		void SetEncoding(mail_encoding encoding, int32 charset);
			//------encoding: you should always use quoted_printable, base64 is strongly not reccomend, see rfc 2047 for the reasons why
			//------charset: use Conversion flavor constants from UTF8.h
		
		void SetText(const char *text);
		void AppendText(const char *text);
		
		const char *Text();
	
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		BString text;
		
		mail_encoding encoding;
		uint32 charset;
};

#endif
