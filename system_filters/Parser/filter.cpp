/* Message Parser - parses the header of incoming e-mail
**
** Copyright 2001-2003 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>
#include <String.h>
#include <E-mail.h>
#include <Locker.h>
#include <malloc.h>

#include <MailAddon.h>
#include <mail_util.h>


using namespace Zoidberg;


class ParseFilter : public Mail::Filter {
	public:
		ParseFilter(BMessage *msg);

		virtual status_t InitCheck(BString *err);
		virtual status_t ProcessMailMessage(BPositionIO **ioMessage, BEntry *ioEntry,
							BMessage *ioHeaders, BPath *ioFolder, const char *io_uid);

	private:
		BString fNameField;
};


ParseFilter::ParseFilter(BMessage *msg)
	: Mail::Filter(msg),
	fNameField("From")
{
	const char *name = msg->FindString("name_field");
	if (name)
		fNameField = name;
}


status_t
ParseFilter::InitCheck(BString* err)
{
	return B_OK;
}


status_t
ParseFilter::ProcessMailMessage(BPositionIO **data, BEntry */*entry*/, BMessage *headers,
	BPath */*folder*/, const char */*uid*/)
{
	char byte;
	(*data)->Seek(SEEK_SET, 0);
	(*data)->Read(&byte, 1);

	status_t status = Mail::parse_header(*headers, **data);
	if (status < B_OK)
		return status;

	//
	// add pseudo-header THREAD, that contains the subject
	// minus stuff in []s (added by mailing lists) and
	// Re: prefixes, added by mailers when you reply.
	// This will generally be the "thread subject".
	//
	BString string;
	string.SetTo(headers->FindString("Subject"));
	Mail::SubjectToThread(string);
	headers->AddString("THREAD", string.String());

	// name
	if (headers->FindString(fNameField.String(), 0, &string) == B_OK) {
		Mail::extract_address_name(string);
		headers->AddString("NAME", string);
	}

	// header length
	headers->AddInt32(B_MAIL_ATTR_HEADER, (int32)((*data)->Position()));
	// What about content length?  If we do that, we have to D/L the
	// whole message...
	//--NathanW says let the disk consumer do that
	
	(*data)->Seek(0, SEEK_SET);
	return B_OK;
}


Mail::Filter *
instantiate_mailfilter(BMessage *settings, Mail::ChainRunner *)
{
	return new ParseFilter(settings);
}

