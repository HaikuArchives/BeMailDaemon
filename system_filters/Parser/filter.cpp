#include <Message.h>
#include <String.h>

#include <MailAddon.h>

#include "mail_parser.h"
#include "mail_util.h"

class ParseFilter: public MailFilter
{
	const mail_header_field* fields;
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
: MailFilter(msg), fields(gDefaultFields), name(msg->FindString("name_field"))
// should get fields from that message
{}
status_t ParseFilter::InitCheck(BString* err){ return B_OK; }

MDStatus ParseFilter::ProcessMailMessage(BPositionIO** data, BEntry*, BMessage* headers, BPath*, BString*)
{
	//BFile *f = reinterpret_cast<BFile*>(*data);
	//if (f) ParseRFC2822File(headers, f, fields);
	   ParseRFC2822File(headers, **data, fields);
	
	headers->PrintToStream();
	
	// name
	BString h;
	if (headers->FindString(name.String(),0,&h)==B_OK)
	{
		StripGook(&h);
		headers->AddString(B_MAIL_ATTR_NAME,h);
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
