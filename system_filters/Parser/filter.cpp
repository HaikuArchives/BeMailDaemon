#include <Message.h>
#include <String.h>
#include <E-mail.h>

#include <MailAddon.h>

#include "mail_util.h"

class ParseFilter: public MailFilter
{
	BString name;
	
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
: MailFilter(msg), name(msg->FindString("name_field"))
{}

status_t ParseFilter::InitCheck(BString* err){ return B_OK; }

MDStatus ParseFilter::ProcessMailMessage(BPositionIO** data, BEntry*, BMessage* headers, BPath*, BString*)
{
	   
	//----Thread messages
	char *		buf = NULL;
	size_t		buflen = 0;
	int32		len;
	time_t		when;
	
	char byte;
	(*data)->Read(&byte,1);
	(*data)->Seek(SEEK_SET,0);
	
	BString string,piece;
	
	//
	// Parse the header
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
		piece.ToLower(); //-------Unified case for later fetch
		
		headers->AddString(piece.String(),string.String() + piece.Length() + 2);
	}
		
	string.SetTo(headers->FindString("subject"));
	int32 last_i = 0, index =0;
		
	while (index < string.Length()) {
		if (string.ByteAt(index) == '[')
			index = string.FindFirst(']')+2;
		
		last_i = string.FindFirst(": ",index) + 2;
		if (last_i < 2)
			break;
		
		string.Remove(index,last_i-index);
	}
	headers->AddString("THREAD",string.String());
	
	headers->PrintToStream();
	
	// name
	BString h;
	if (headers->FindString(name.String(),0,&h)==B_OK)
	{
		StripGook(&h);
		headers->AddString("NAME",h);
	}
	
	// header length
	headers->AddInt32(B_MAIL_ATTR_HEADER, (int32)((*data)->Position()));
	// What about content length?  If we do that, we have to D/L the
	// whole message...
	//--NathanW says let the disk consumer do that
	
	(*data)->Seek(0,SEEK_SET);
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView *view)
{ return new ParseFilter(settings); }
