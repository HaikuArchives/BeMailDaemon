#include <String.h>
#include <Mime.h>

#include <malloc.h>
#include <ctype.h>

class _EXPORT MailComponent;
class _EXPORT PlainTextBodyComponent;

#include <MailComponent.h>
#include <MailAttachment.h>
#include <MailContainer.h>
#include <mail_util.h>

#include <base64.h>
#include <qp.h>

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

extern const CharsetConversionEntry charsets[21];

MailComponent::MailComponent() {}
		
MailComponent *MailComponent::WhatIsThis() {
	BMimeType type, super;
	MIMEType(&type);
	type.GetSupertype(&super);
	
	puts(type.Type());
	
	MailComponent *piece;
	//---------ATT-This code *desperately* needs to be improved
	if (super == "multipart") {
		if (type == "multipart/x-bfile")
			piece = new AttributedMailAttachment;
		else
			piece = new MIMEMultipartContainer;
	} else {
		if (IsAttachment())
			piece = new PlainTextBodyComponent;
		else
			piece = new SimpleMailAttachment;
	}
	
	return piece;
}

bool MailComponent::IsAttachment() {
	const char *disposition = HeaderField("Content-Disposition");
	if ((disposition != NULL) && (strncasecmp(disposition,"Attachment",strlen("Attachment")) == 0))
		return true;
	
	BString header = HeaderField("Content-Type");
	if (header.FindFirst("name=") && (header.FindFirst(";") > 0) && (header.FindFirst("name=") > header.FindFirst(";")))
		return true;
		
	BMimeType type;
	MIMEType(&type);
	if (type == "multipart/x-bfile")
		return true;
	
	return false;
}

void MailComponent::SetHeaderField(const char *key, const char *value, uint32 charset, mail_encoding encoding, bool replace_existing) {
	if (replace_existing)
		headers.RemoveName(key);
	
	headers.AddString(key,value);
	headers.AddInt32(key,charset);
	headers.AddInt8(key,encoding);
}

void MailComponent::SetHeaderField(const char *key, BMessage *structure, bool replace_existing) {
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
	
const char *MailComponent::HeaderField(const char *key, int32 index) {
	const char *string;
	
	headers.FindString(key,index,&string);
	return string;
}

status_t MailComponent::HeaderField(const char *key, BMessage *structure, int32 index) {
	BString string,sub_cat,end_piece;
	
	string = HeaderField(key,index);
	if (string == "")
		return B_NAME_NOT_FOUND;
	
	int32 i = 0, end = 0;
	while (end < string.Length()) {
		end = string.FindFirst(';',i);
		if (end < 0)
			end = string.Length();
			
		string.CopyInto(sub_cat,i,end - i);
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
			structure->AddString(sub_cat.String(),end_piece.String());
		} else {
			structure->AddString("unlabeled",sub_cat.String());
		}
	}
		
	return B_OK;
}

status_t MailComponent::RemoveHeader(const char *key) {
	return headers.RemoveName(key);
}

status_t MailComponent::GetDecodedData(BPositionIO *) {return B_OK;}
status_t MailComponent::SetDecodedData(BPositionIO *) {return B_OK;}

status_t MailComponent::Instantiate(BPositionIO *data, size_t length) {
	headers.MakeEmpty();
	
	BString string,piece;
	char *		buf = (char *)malloc(1);
	size_t		buflen = 0;
	int32		len;
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
		string.SetTo(buf);
		
		if (string.FindFirst(": ") < 0)
			continue;
		
		string.CopyInto(piece,0,string.FindFirst(": "));
		//piece.ToLower(); //-------Unified case for later fetch
		
		headers.AddString(piece.String(),string.String() + piece.Length() + 2);
	}
		
	free(buf);

	length = 0; // Remove the warning. Is length necessary at all?
	
	return B_OK;
}
	
status_t MailComponent::Render(BPositionIO *render_to) {
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

status_t MailComponent::MIMEType(BMimeType *mime) {
	BMessage msg;
	HeaderField("Content-Type",&msg);
	
	mime->SetTo(msg.FindString("unlabeled"));
	
	return B_OK;
}

MailComponent::~MailComponent() {}

PlainTextBodyComponent::PlainTextBodyComponent(const char *text) 
	: MailComponent(),
	encoding(quoted_printable),
	charset(B_ISO1_CONVERSION) {
		if (text != NULL)
			SetText(text);
			
		SetHeaderField("MIME-Version","1.0");
}

void PlainTextBodyComponent::SetEncoding(mail_encoding encoding, int32 charset) {
	this->encoding = encoding;
	this->charset = charset;
}

void PlainTextBodyComponent::SetText(const char *text) {
	this->text.SetTo(text);
}

void PlainTextBodyComponent::AppendText(const char *text) {
	this->text << text;
}

const char *PlainTextBodyComponent::Text() {
	return text.String();
}

BString *PlainTextBodyComponent::BStringText() {
	return &text;
}

void PlainTextBodyComponent::Quote(const char *message, const char *quote_style) {
	BString string;
	string << '\n' << quote_style;
	text.ReplaceAll("\n",string.String());
	
	string = message;
	string << '\n';
	text.Prepend(string.String());
}

status_t PlainTextBodyComponent::GetDecodedData(BPositionIO *data) {
	data->Write(text.String(),text.Length());
	return B_OK;
}

status_t PlainTextBodyComponent::SetDecodedData(BPositionIO *data)  {
	char buffer[255];
	size_t buf_len;
	
	while ((buf_len = data->Read(buffer,254)) > 0) {
		buffer[buf_len] = 0;
		this->text << buffer;
	}
	
	return B_OK;
}

status_t PlainTextBodyComponent::Instantiate(BPositionIO *data, size_t length) {
	off_t position = data->Position();
	MailComponent::Instantiate(data,length);
	
	length -= (data->Position() - position);

	//printf("Position %d, new position %d, length %d\n",(int32)position,(int32)data->Position(),length);
	
	//--------Note: the following code blows up on MIME components. Not sure how to fix.
	/*if (HeaderField("MIME-Version") == NULL) {
		text = "";
		char buffer[255];
		size_t buf_len;
		for (int32 offset = 0; (buf_len = data->Read(buffer,((length - offset) >= 254) ? 254 : (length - offset))) > 0; offset += buf_len) { 
			buffer[buf_len] = 0;
			text << buffer;
		}
		return;
	}*/
	
	BString content_type = HeaderField("Content-Type");
	content_type.Truncate(content_type.FindFirst("; ") + 2);
	
	charset = B_ISO1_CONVERSION;
	for (int32 i = 0; i < 21; i++) {
		if (content_type == charsets[i].charset) {
			charset = charsets[i].flavor;
			break;
		}
	}
	
	content_type = HeaderField("Content-Transfer-Encoding");
	encoding = no_encoding;
	
	if (content_type.IFindFirst("base64") >= 0)
		encoding = base64;
	if (content_type.IFindFirst("quoted-printable") >= 0)
		encoding = quoted_printable;
	
	char buffer[255];
	size_t buf_len;
	
	BString alternate, alt2;
	for (int32 offset = 0; (buf_len = data->Read(buffer,((length - offset) >= 254) ? 254 : (length - offset))) > 0; offset += buf_len) { 
		buffer[buf_len] = 0;
		alternate << buffer;
	}
	
	ssize_t len;
	char *text = alt2.LockBuffer(alternate.Length()+1);
	switch (encoding) {
		case 'b':
			len = decode_base64(text,alternate.String(),alternate.Length());
			text[len] = 0;
			break;
		case 'q':
			len = decode_qp(text,alternate.String(),alternate.Length());
			text[len] = 0;
			break;
		default:
			len = alternate.Length();
			strcpy(text,alternate.String());
	}
	alt2.UnlockBuffer(len+1);
	
	alt2.ReplaceAll("\r\n","\n");
	
	text = this->text.LockBuffer(len * 2);
	int32 dest_len = len * 2;
	int32 state;
	convert_to_utf8(charset,alt2.String(),&len,text,&dest_len,&state);
	text[dest_len] = 0;
	this->text.UnlockBuffer(dest_len+1);
	
	return B_OK;
}

status_t PlainTextBodyComponent::Render(BPositionIO *render_to) {
	BString content_type;
	content_type << "text/plain; ";
	
	for (uint32 i = 0; i < sizeof(charsets); i++) {
		if (charsets[i].flavor == charset) {
			content_type << "charset=\"" << charsets[i].charset << "\"";
			break;
		}
	}
	
	SetHeaderField("Content-Type",content_type.String());
	
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
	
	MailComponent::Render(render_to);
	
	BString modified = this->text;
	BString alt;
	
	int32 len = this->text.Length();
	if (len > 0) {
		int32 dest_len = len * 2;
		char *raw = alt.LockBuffer(dest_len);
		int32 state;
		convert_from_utf8(charset,this->text.String(),&len,raw,&dest_len,&state);
		raw[dest_len] = 0;
		alt.UnlockBuffer(dest_len);
		
		raw = modified.LockBuffer((alt.Length()*3)+1);
		switch (encoding) {
			case 'b':
				len = encode_base64(raw,alt.String(),alt.Length());
				text[len] = 0;
				break;
			case 'q':
				len = encode_qp(raw,alt.String(),alt.Length(), false);
				text[len] = 0;
				break;
			default:
				len = alt.Length();
				strcpy(raw,alt.String());
		}
		modified.UnlockBuffer(len);
		
		//------Desperate bid to wrap lines
		modified.ReplaceAll("\n","\r\n");
		
		int32 curr_line_length = 0;
		int32 last_space = 0;
		
		for (int32 i = 0; i < modified.Length(); i++) {
			if (isspace(modified.ByteAt(i)))
				last_space = i;
				
			if ((modified.ByteAt(i) == '\r') && (modified.ByteAt(i+1) == '\n'))
				curr_line_length = 0;
			else
				curr_line_length++;
				
			if (curr_line_length > 80) {
				if (last_space >= 0) {
					modified.Insert("\r\n",last_space);
					last_space = -1;
					curr_line_length = 0;
				}
			}
		}
	}	
	modified << "\r\n";
	
	render_to->Write(modified.String(),modified.Length());
	
	return B_OK;
}

PlainTextBodyComponent::~PlainTextBodyComponent() {}
