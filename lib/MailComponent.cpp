#include <String.h>
#include <Mime.h>

#include <malloc.h>
#include <ctype.h>

class _EXPORT MailComponent;
class _EXPORT PlainTextBodyComponent;

#include <MailComponent.h>
#include <mail_util.h>

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
		piece.ToLower(); //-------Unified case for later fetch
		
		headers.AddString(piece.String(),string.String() + piece.Length() + 2);
	}
		
	free(buf);
	
	return B_OK;
}
	
status_t MailComponent::Render(BPositionIO *render_to) {
	int32 charset;
	int8 encoding;
	char *key, *value, *allocd;
	
	BString concat;
	
	type_code stupidity_personified = B_STRING_TYPE;
	int32 count = 0;
	
	for (int32 index = 0; headers.GetInfo(B_STRING_TYPE,index,&key,&stupidity_personified,&count) == B_OK; index++) {
		for (int32 g = 0; g < count; g++) {
			headers.FindString(key,g,&value);
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
	
	SetText(data);
	
	return B_OK;
}

status_t PlainTextBodyComponent::Render(BPositionIO *render_to) {
	MailComponent::Render(render_to);
	
	char *rfc2047 = (char *)malloc(text.Length() + 1);
	strcpy(rfc2047,text.String());
	
	utf8_to_rfc2047(&rfc2047,text.Length(),charset,encoding);
	
	//------Desperate bid to wrap lines
	BString modified = rfc2047;
	free(rfc2047);
	
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