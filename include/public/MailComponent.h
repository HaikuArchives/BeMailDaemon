#ifndef ZOIDBERG_MAIL_COMPONENT_H
#define ZOIDBERG_MAIL_COMPONENT_H
/* (Text)Component - message component base class and plain text
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <UTF8.h>
#include <Message.h>
#include <String.h>

#include <mail_encoding.h>

class BMimeType;


namespace Zoidberg {
namespace Mail {

enum component_type {
	MC_PLAIN_TEXT_BODY = 0,
	MC_SIMPLE_ATTACHMENT,
	MC_ATTRIBUTED_ATTACHMENT,
	MC_MULTIPART_CONTAINER
};

class Component {
	public:
		Component();
		virtual ~Component();
		
		//------Info on this component
		uint32 ComponentType();
		Component *WhatIsThis();
			// Takes any generic MailComponent, and returns an instance
			// of a MailComponent subclass that applies to this case,
			// ready for instantiation. Note that you still have to
			// Instantiate() it yourself.
		bool IsAttachment();
			// Returns true if this component is an attachment, false
			// otherwise

		void SetHeaderField(
			const char *key, const char *value,
			uint32 charset = B_ISO1_CONVERSION,
			mail_encoding encoding = quoted_printable,
			bool replace_existing = true);
		void SetHeaderField(
			const char *key, BMessage *structured_header,
			bool replace_existing = true);
			
		const char *HeaderAt(int32 index);
		const char *HeaderField(const char *key, int32 index = 0);
		status_t	HeaderField(const char *key, BMessage *structured_header, int32 index = 0);
		
		status_t	RemoveHeader(const char *key);
		
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	
	private:
		BMessage headers;
};

class TextComponent : public Mail::Component {
	public:
		TextComponent(const char *text = NULL);
		virtual ~TextComponent();		

		void SetEncoding(mail_encoding encoding, int32 charset);
			//------encoding: you should always use quoted_printable, base64 is strongly not reccomend, see rfc 2047 for the reasons why
			//------charset: use Conversion flavor constants from UTF8.h
		
		void SetText(const char *text);
		void AppendText(const char *text);
		
		const char *Text();
		BString *BStringText();
		
		void Quote(const char *message = NULL,
				   const char *quote_style = "> ");
	
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);
	private:
		BString text;
		BString decoded;
		
		mail_encoding encoding;
		uint32 charset;
		
		void ParseRaw();
		BPositionIO *raw_data;
		size_t raw_length;
		off_t raw_offset;
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_COMPONENT_H */
