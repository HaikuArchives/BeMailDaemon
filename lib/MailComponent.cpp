#include <String.h>
#include <Mime.h>

#include <malloc.h>

class _EXPORT MailComponent;

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