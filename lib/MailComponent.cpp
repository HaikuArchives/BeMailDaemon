#include <String.h>
#include <Mime.h>

#include <malloc.h>
#include <ctype.h>

class _EXPORT MailComponent;
class _EXPORT PlainTextBodyComponent;

#include <MailComponent.h>
#include <mail_util.h>

#include <base64.h>
#include <qp.h>

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

static const CharsetConversionEntry charsets[] =
{
	{"iso-8859-1",  B_ISO1_CONVERSION},
	{"iso-8859-2",  B_ISO2_CONVERSION},
	{"iso-8859-3",  B_ISO3_CONVERSION},
	{"iso-8859-4",  B_ISO4_CONVERSION},
	{"iso-8859-5",  B_ISO5_CONVERSION},
	{"iso-8859-6",  B_ISO6_CONVERSION},
	{"iso-8859-7",  B_ISO7_CONVERSION},
	{"iso-8859-8",  B_ISO8_CONVERSION},
	{"iso-8859-9",  B_ISO9_CONVERSION},
	{"iso-8859-10", B_ISO10_CONVERSION},
	{"iso-2022-jp", B_JIS_CONVERSION},
	{"koi8-r",      B_KOI8R_CONVERSION},
	{"iso-8859-13", B_ISO13_CONVERSION},
	{"iso-8859-14", B_ISO14_CONVERSION},
	{"iso-8859-15", B_ISO15_CONVERSION},
	{"Windows-1251",B_MS_WINDOWS_1251_CONVERSION},
	{"Windows-1252",B_MS_WINDOWS_CONVERSION},
	{"dos-866",     B_MS_DOS_866_CONVERSION},
	{"dos-437",     B_MS_DOS_CONVERSION},
	{"euc-kr",      B_EUC_KR_CONVERSION},
	{"x-mac-roman", B_MAC_ROMAN_CONVERSION}
};

MailComponent::MailComponent() {}
		
void MailComponent::AddHeaderField(const char *key, const char *value, uint32 charset, char encoding, bool replace_existing) {
	if (replace_existing)
		headers.RemoveName(key);
	
	headers.AddString(key,value);
	headers.AddInt32(key,charset);
	headers.AddInt8(key,encoding);
}
	
const char *MailComponent::HeaderField(const char *key, int32 index) {
	const char *string;
	
	headers.FindString(key,index,&string);
	return string;
}

status_t MailComponent::Instantiate(BPositionIO *data, size_t length) {
	BString string,piece;
	char *		buf = NULL;
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
	BString string = HeaderField("content-type");
	string.Truncate(string.FindFirst(';'));
	mime->SetTo(string.String());
	
	return B_OK;
}

MailComponent::~MailComponent() {}

PlainTextBodyComponent::PlainTextBodyComponent(const char *text) 
	: MailComponent(),
	encoding('q'),
	charset(B_ISO1_CONVERSION) {
		if (text != NULL)
			SetText(text);
}

void PlainTextBodyComponent::SetEncoding(char encoding, int32 charset) {
	this->encoding = encoding;
	this->charset = charset;
}

void PlainTextBodyComponent::SetText(const char *text) {
	this->text.SetTo(text);
	
	this->text.ReplaceAll("\r\n","\n");
}

void PlainTextBodyComponent::SetText(BDataIO *text) {
	char buffer[255];
	size_t buf_len;
	
	while ((buf_len = text->Read(buffer,254)) > 0) {
		buffer[buf_len] = 0;
		this->text << buffer;
	}
	
	this->text.ReplaceAll("\r\n","\n");
}

void PlainTextBodyComponent::AppendText(const char *text) {
	this->text << text;
}

const char *PlainTextBodyComponent::Text() {
	return text.String();
}

status_t PlainTextBodyComponent::Instantiate(BPositionIO *data, size_t length) {
	MailComponent::Instantiate(data,length);
	
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
	encoding = -1;
	
	if (content_type.IFindFirst("base64") >= 0)
		encoding = 'b';
	if (content_type.IFindFirst("quoted-printable") >= 0)
		encoding = 'q';
	
	char buffer[255];
	size_t buf_len;
	
	BString alternate, alt2;
	for (int32 offset = 0; (buf_len = data->Read(buffer,((length - offset) >= 254) ? 254 : (length - offset))) > 0; offset++) { 
		buffer[buf_len] = 0;
		alternate << buffer;
	}
	
	alternate.ReplaceAll("\r\n","\n");
	
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
	
	AddHeaderField("Content-Type",content_type.String());
	
	const char *transfer_encoding = NULL;
	switch (encoding) {
		case 'b':
			transfer_encoding = "base64";
			break;
		case 'q':
			transfer_encoding = "quoted-printable";
			break;
		default:
			transfer_encoding = "7bit";
			break;
	}
	
	AddHeaderField("Content-Transfer-Encoding",transfer_encoding);
	
	MailComponent::Render(render_to);
	
	BString modified;
	BString alt;
	
	int32 len = this->text.Length();
	int32 dest_len = len * 2;
	char *raw = alt.LockBuffer(dest_len);
	int32 state;
	convert_from_utf8(charset,this->text.String(),&len,raw,&dest_len,&state);
	raw[dest_len] = 0;
	alt.UnlockBuffer(dest_len + 1);
	
	raw = modified.LockBuffer(alt.Length()+1);
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
	modified.UnlockBuffer(len+1);
	
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
			modified.Insert("\r\n",last_space+1);
			i = last_space;
			curr_line_length = 0;
		}
	}
	
	render_to->Write(modified.String(),modified.Length());
	
	return B_OK;
}

PlainTextBodyComponent::~PlainTextBodyComponent() {}
