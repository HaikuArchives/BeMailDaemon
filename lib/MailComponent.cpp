/* (Text)Component - message component base class and plain text
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <String.h>
#include <Mime.h>

#include <malloc.h>
#include <ctype.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT Component;
	class _EXPORT TextComponent;
}
}

#include <MailComponent.h>
#include <MailAttachment.h>
#include <MailContainer.h>
#include <mail_util.h>


namespace Zoidberg {
namespace Mail {

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

extern const CharsetConversionEntry charsets[21];


Component::Component()
{
}

Component::~Component()
{
}

uint32 Component::ComponentType()
{
	BMimeType type, super;
	MIMEType(&type);
	type.GetSupertype(&super);

	//---------ATT-This code *desperately* needs to be improved
	if (super == "multipart") {
		if (type == "multipart/x-bfile")
			return MC_ATTRIBUTED_ATTACHMENT;
		else
			return MC_MULTIPART_CONTAINER;
	} else if (!IsAttachment() && (super == "text" || type.Type() == NULL))
		return MC_PLAIN_TEXT_BODY;
	else
		return MC_SIMPLE_ATTACHMENT;
}

Component *Component::WhatIsThis() {
	switch (ComponentType())
	{
		case MC_SIMPLE_ATTACHMENT:
			return new Mail::SimpleAttachment;
		case MC_ATTRIBUTED_ATTACHMENT:
			return new Mail::AttributedAttachment;
		case MC_MULTIPART_CONTAINER:
			return new Mail::MIMEMultipartContainer;
		case MC_PLAIN_TEXT_BODY:
		default:
			return new TextComponent;
	}
}

bool Component::IsAttachment() {
	const char *disposition = HeaderField("Content-Disposition");
	if ((disposition != NULL) && (strncasecmp(disposition,"Attachment",strlen("Attachment")) == 0))
		return true;
	
	BMessage header;
	HeaderField("Content-Type",&header);
	if (header.HasString("name"))
		return true;
	
	if (HeaderField("Content-Location",&header) == B_OK)
		return true;

	BMimeType type;
	MIMEType(&type);
	if (type == "multipart/x-bfile")
		return true;
	
	return false;
}
	
void Component::SetHeaderField(const char *key, const char *value, uint32 charset, mail_encoding encoding, bool replace_existing) {
	if (replace_existing)
		headers.RemoveName(key);
	
	headers.AddString(key,value);
	headers.AddInt32(key,charset);
	headers.AddInt8(key,encoding);
}

void Component::SetHeaderField(const char *key, BMessage *structure, bool replace_existing) {
	if (replace_existing)
		headers.RemoveName(key);
	
	BString value;
	if (structure->HasString("unlabeled"))
		value << structure->FindString("unlabeled") << "; ";
	
	const char *name, *sub_val;
	type_code type;
	for (int32 i = 0; structure->GetInfo(B_STRING_TYPE,i,
		#ifndef B_BEOS_VERSION_DANO
			(char**)
		#endif
		&name,&type) == B_OK; i++)
	{
		if (strcasecmp(name,"unlabeled") == 0)
			continue;
		
		structure->FindString(name, &sub_val);
		value << name << '=';
		if (BString(sub_val).FindFirst(' ') > 0)
			value << '\"' << sub_val << "\"; ";
		else
			value << sub_val << "; ";
	}
	
	value.Truncate(value.Length() - 2); //-----Remove the last "; "
	
	SetHeaderField(key,value.String(),B_ISO1_CONVERSION,no_encoding);
}
	
const char *Component::HeaderField(const char *key, int32 index) {
	const char *string = NULL;
	
	headers.FindString(key,index,&string);
	return string;
}

status_t Component::HeaderField(const char *key, BMessage *structure, int32 index) {
	BString string = HeaderField(key,index);
	if (string == "")
		return B_NAME_NOT_FOUND;
	
	BString sub_cat,end_piece;
	int32 i = 0, end = 0;

	while (end < string.Length()) {
		end = string.FindFirst(';',i);
		if (end < 0)
			end = string.Length();
		
		string.CopyInto(sub_cat,i,end - i);
		i = end + 1;

		//-------Trim spaces off of beginning and end of text
		for (int32 h = 0; h < sub_cat.Length(); h++) {
			if (!isspace(sub_cat.ByteAt(h))) {
				sub_cat.Remove(0,h);
				break;
			}
		}
		for (int32 h = sub_cat.Length()-1; h >= 0; h--) {
			if (!isspace(sub_cat.ByteAt(h))) {
				sub_cat.Truncate(h+1);
				break;
			}
		}
		//--------Split along '='
		int32 first_equal = sub_cat.FindFirst('=');
		if (first_equal >= 0) {
			sub_cat.CopyInto(end_piece,first_equal+1,sub_cat.Length() - first_equal - 1);
			sub_cat.Truncate(first_equal);
			if (end_piece.ByteAt(0) == '\"') {
				end_piece.Remove(0,1);
				end_piece.Truncate(end_piece.Length() - 1);
			}
			sub_cat.ToLower();
			structure->AddString(sub_cat.String(),end_piece.String());
		} else {
			structure->AddString("unlabeled",sub_cat.String());
		}
	}
		
	return B_OK;
}

status_t Component::RemoveHeader(const char *key) {
	return headers.RemoveName(key);
}

const char *Component::HeaderAt(int32 index) {
#if B_BEOS_VERSION_DANO
	const
#endif
	char *name = NULL;
	type_code type;
	
	headers.GetInfo(B_STRING_TYPE,index,&name,&type);
	return name;
}

status_t Component::GetDecodedData(BPositionIO *) {return B_OK;}
status_t Component::SetDecodedData(BPositionIO *) {return B_OK;}

status_t
Component::SetToRFC822(BPositionIO *data, size_t /*length*/, bool /* parse_now */)
{
	headers.MakeEmpty();

	char *	buf = NULL;
	size_t	buflen = 0;
	int32	len;
	//
	// Parse the header
	//
	while ((len = readfoldedline(*data, &buf, &buflen)) > 2)
	{
		if (buf[len-2] == '\r') len -= 2;
		else if (buf[len-1] == '\n') --len;
				
		// convert to UTF-8
		len = rfc2047_to_utf8(&buf, &buflen, len);
		
		// terminate
		buf[len] = 0;

		const char *delimiter = strstr(buf, ": ");
		if (delimiter == NULL)
			continue;	

		BString header(buf, delimiter - buf);
		header.CapitalizeEachWord(); //-------Unified case for later fetch

		headers.AddString(header.String(),delimiter + 2);
	}
	if (buf)
		free(buf);

	return B_OK;
}
	
status_t Component::RenderToRFC822(BPositionIO *render_to) {
	int32 charset;
	int8 encoding;
	const char *key, *value;
	char *allocd;
	
	BString concat;
	
	type_code stupidity_personified = B_STRING_TYPE;
	int32 count = 0;
	
	for (int32 index = 0; headers.GetInfo(B_STRING_TYPE,index,
#ifndef B_BEOS_VERSION_DANO
	(char**)
#endif
				&key,&stupidity_personified,&count) == B_OK; index++) {
		for (int32 g = 0; g < count; g++) {
			headers.FindString(key,g,(const char **)&value);
			allocd = (char *)malloc(strlen(value) + 1);
			strcpy(allocd,value);
			
			if (headers.FindInt32(key,&charset) != B_OK)
				charset = B_ISO1_CONVERSION;
				
			if (headers.FindInt8(key,&encoding) != B_OK)
				encoding = 'q';
			
			concat << key << ": ";
			concat.CapitalizeEachWord();
			
			concat.Append(allocd,utf8_to_rfc2047(&allocd, strlen(value), charset, encoding));
			
			concat << "\r\n";
			
			free(allocd);
			
			render_to->Write(concat.String(), concat.Length());
			concat = "";
		}
	}
	
	render_to->Write("\r\n", 2);
	
	return B_OK;
}

status_t Component::MIMEType(BMimeType *mime) {
	BMessage msg;
	HeaderField("Content-Type",&msg);
	
	mime->SetTo(msg.FindString("unlabeled"));
	
	return B_OK;
}


void Component::_ReservedComponent1() {}
void Component::_ReservedComponent2() {}
void Component::_ReservedComponent3() {}
void Component::_ReservedComponent4() {}
void Component::_ReservedComponent5() {}


//-------------------------------------------------------------------------
//	#pragma mark -


TextComponent::TextComponent(const char *text) 
	: Component(),
	encoding(quoted_printable),
	charset(B_ISO1_CONVERSION),
	raw_data(NULL)
{
	if (text != NULL)
		SetText(text);
			
	SetHeaderField("MIME-Version","1.0");
}

TextComponent::~TextComponent()
{
}

void TextComponent::SetEncoding(mail_encoding encoding, int32 charset) {
	this->encoding = encoding;
	this->charset = charset;
}

void TextComponent::SetText(const char *text) {
	this->text.SetTo(text);
	
	raw_data = NULL;
}

void TextComponent::AppendText(const char *text) {
	ParseRaw();
	
	this->text << text;
}

const char *TextComponent::Text() {
	ParseRaw();
	
	return text.String();
}

BString *TextComponent::BStringText() {
	ParseRaw();
	
	return &text;
}

void TextComponent::Quote(const char *message, const char *quote_style) {
	ParseRaw();
	
	BString string;
	string << '\n' << quote_style;
	text.ReplaceAll("\n",string.String());
	
	string = message;
	string << '\n';
	text.Prepend(string.String());
}

status_t TextComponent::GetDecodedData(BPositionIO *data) {
	ParseRaw();
	
	BMimeType type;
	ssize_t written;
	if (MIMEType(&type) == B_OK && type == "text/plain")
		written = data->Write(text.String(),text.Length());
	else
		written = data->Write(decoded.String(), decoded.Length());

	return written >= 0 ? B_OK : written;
}

status_t TextComponent::SetDecodedData(BPositionIO *data)  {
	char buffer[255];
	size_t buf_len;
	
	while ((buf_len = data->Read(buffer,254)) > 0) {
		buffer[buf_len] = 0;
		this->text << buffer;
	}
	
	raw_data = NULL;
	
	return B_OK;
}


status_t
TextComponent::SetToRFC822(BPositionIO *data, size_t length, bool parseNow)
{
	off_t position = data->Position();
	Component::SetToRFC822(data, length);

	length -= data->Position() - position;

	raw_data = data;
	raw_length = length;
	raw_offset = data->Position();

	if (parseNow) {
		// copies the data stream and sets the raw_data variable to NULL
		return ParseRaw();
	}

	return B_OK;
}
		

status_t
TextComponent::ParseRaw()
{
	if (raw_data == NULL)
		return B_OK;

	raw_data->Seek(raw_offset, SEEK_SET);

	BMessage content_type;
	HeaderField("Content-Type", &content_type);

	charset = B_ISO1_CONVERSION;
	if (content_type.HasString("charset")) {
		for (int32 i = 0; i < 21; i++) {
			if (strcasecmp(content_type.FindString("charset"), charsets[i].charset) == 0) {
				charset = charsets[i].flavor;
				break;
			}
		}
	}

	encoding = encoding_for_cte(HeaderField("Content-Transfer-Encoding"));

	char *buffer = (char *)malloc(raw_length + 1);
	if (buffer == NULL)
		return B_NO_MEMORY;

	ssize_t bytes;
	if ((bytes = raw_data->Read(buffer, raw_length)) < 0)
		return B_IO_ERROR;

	char *string = decoded.LockBuffer(bytes + 1);
	bytes = decode(encoding, string, buffer, bytes, 0);
	decoded.UnlockBuffer(bytes);
	decoded.ReplaceAll("\r\n", "\n");

	string = text.LockBuffer(bytes * 2);
	int32 destLength = bytes * 2;
	int32 state;
	convert_to_utf8(charset, decoded.String(), &bytes, string, &destLength, &state);
	if (destLength > 0)
		text.UnlockBuffer(destLength);
	else {
		text.UnlockBuffer(0);
		text.SetTo(decoded);
	}

	raw_data = NULL;
	return B_OK;
}


status_t
TextComponent::RenderToRFC822(BPositionIO *render_to)
{
	status_t status = ParseRaw();
	if (status < B_OK)
		return status;

	BString content_type;
	content_type << "text/plain; ";

	for (uint32 i = 0; i < sizeof(charsets); i++) {
		if (charsets[i].flavor == charset) {
			content_type << "charset=\"" << charsets[i].charset << "\"";
			break;
		}
	}

	SetHeaderField("Content-Type", content_type.String());

	const char *transfer_encoding = NULL;
	switch (encoding) {
		case base64:
			transfer_encoding = "base64";
			break;
		case quoted_printable:
			transfer_encoding = "quoted-printable";
			break;
		default:
			transfer_encoding = "7bit";
			break;
	}
	
	SetHeaderField("Content-Transfer-Encoding",transfer_encoding);
	
	Component::RenderToRFC822(render_to);
	
	BString modified = this->text;
	BString alt;
	
	int32 len = this->text.Length();
	if (len > 0) {
		int32 dest_len = len * 2;
		char *raw = alt.LockBuffer(dest_len);
		int32 state;
		convert_from_utf8(charset,this->text.String(),&len,raw,&dest_len,&state);
		alt.UnlockBuffer(dest_len);
		
		raw = modified.LockBuffer((alt.Length()*3)+1);
		switch (encoding) {
			case 'b':
				len = encode_base64(raw,alt.String(),alt.Length());
				raw[len] = 0;
				break;
			case 'q':
				len = encode_qp(raw,alt.String(),alt.Length(), false);
				raw[len] = 0;
				break;
			default:
				len = alt.Length();
				strcpy(raw,alt.String());
		}
		modified.UnlockBuffer(len);

		modified.ReplaceAll("\n","\r\n");

		// There seem to be a possibility of NULL bytes in the text,
		// so lets filter them out

		char *string = modified.LockBuffer(modified.Length());
		for (int32 i = modified.Length();i-- > 0;)
		{
			if (string[i] != '\0')
				continue;

			puts("NULL byte in text!!");
			string[i] = ' ';
		}
		modified.UnlockBuffer();

		// word wrapping is already done by BeMail (user-configurable)
		// and it does it *MUCH* nicer.

//		//------Desperate bid to wrap lines
//		int32 curr_line_length = 0;
//		int32 last_space = 0;
//		
//		for (int32 i = 0; i < modified.Length(); i++) {
//			if (isspace(modified.ByteAt(i)))
//				last_space = i;
//				
//			if ((modified.ByteAt(i) == '\r') && (modified.ByteAt(i+1) == '\n'))
//				curr_line_length = 0;
//			else
//				curr_line_length++;
//				
//			if (curr_line_length > 80) {
//				if (last_space >= 0) {
//					modified.Insert("\r\n",last_space);
//					last_space = -1;
//					curr_line_length = 0;
//				}
//			}
//		}
	}	
	modified << "\r\n";
	
	render_to->Write(modified.String(),modified.Length());
	
	return B_OK;
}


void TextComponent::_ReservedText1() {}
void TextComponent::_ReservedText2() {}

}	// namespace Mail
}	// namespace Zoidberg
