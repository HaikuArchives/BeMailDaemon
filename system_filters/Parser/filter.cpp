#include <Message.h>
#include <String.h>
#include <Locker.h>
#include <E-mail.h>

#include <malloc.h>
#include <ctype.h>

#include <MailAddon.h>
#include <mail_util.h>

#define MAX_PREFIX_LENGTH 4

void StripSubjectPrefixes(BString &string);

class ParseFilter: public MailFilter
{
	BString name_field;
	
  public:
	ParseFilter(BMessage*);
	virtual status_t InitCheck(BString *err);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
};

ParseFilter::ParseFilter(BMessage* msg)
: MailFilter(msg), name_field("from")
{
	const char *n = msg->FindString("name_field");
	if (n) name_field = n;
}

status_t ParseFilter::InitCheck(BString* err){ return B_OK; }

MDStatus ParseFilter::ProcessMailMessage(BPositionIO** data, BEntry*, BMessage* headers, BPath*, BString*)
{
	char *		buf = NULL;
	size_t		buflen = 0;
	int32		len;
	time_t		when;
	
	char byte;
	(*data)->Read(&byte,1);
	(*data)->Seek(SEEK_SET,0);
	
	BString string,piece;
	
	//
	// Parse the header.  Add each header as a string to
	// the headers message, under the header's name.
	//
	while ((len = readfoldedline(**data, &buf, &buflen)) > 2)
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
		piece.ToLower(); // Unified case for later fetch
		
		// Add each header to the headers message
		headers->AddString(piece.String(),string.String() + piece.Length() + 2);
	}
		
	free(buf);
	
	//
	// add pseudo-header THREAD, that contains the subject
	// minus stuff in []s (added by mailing lists) and
	// Re: prefixes, added by mailers when you reply.
	// This will generally be the "thread subject".
	//
	string.SetTo(headers->FindString("subject"));
	StripSubjectPrefixes(string);
	headers->AddString("THREAD",string.String());
	
	// name
	if (headers->FindString(name_field.String(),0,&string)==B_OK)
	{
		StripGook(&string);
		headers->AddString("NAME",string);
	}
	
	// header length
	headers->AddInt32(B_MAIL_ATTR_HEADER, (int32)((*data)->Position()));
	// What about content length?  If we do that, we have to D/L the
	// whole message...
	//--NathanW says let the disk consumer do that
	
	(*data)->Seek(0,SEEK_SET);
	return MD_OK;
}

void StripSubjectPrefixes(BString &string) {
	BString sub;
	int32 i = 0,j = 0;
	while(1) {
		if ((string.ByteAt(0) == '[') && (string.FindFirst(']',i) >= 0)) {
			i = string.FindFirst(']')+1;
			if (!isspace(string.ByteAt(i)))
				string.Insert(" ",i);
			i++;
		}
		
		j = string.FindFirst(':',i);
		if (j < 0)
			break;
		j++;
		
		//-------Strip [*]	
		string.CopyInto(sub,i,j-i);
		if ((sub.FindFirst('[') >= 0) && (sub.FindFirst(']') >= 0))
			sub.Remove(sub.FindFirst('['),sub.FindFirst(']') - sub.FindFirst('[') + 1);
		
		//-------Strip white space between x and :, so Re : becomes Re:
		int32 h = sub.Length()-2;
		while (h >= 0) {
			 if (!isspace(sub.ByteAt(h)))
			 	break;
			 	
			 h--;
		}
		sub.Remove(h+1, sub.Length() - 2 - h);
		
		//-------If it is the right length, toss it
		if (sub.CountChars() <= MAX_PREFIX_LENGTH) { //-------Maximum four UTF8 chars including colon
			if (isspace(string.ByteAt(j)))
				j++;
			string.Remove(i,j-i);
		} else {
			break;
		}
			
		sub = "";
	}
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView *view)
{ return new ParseFilter(settings); }
