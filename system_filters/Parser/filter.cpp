/* Message Parser - parses the header of incoming e-mail
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>
#include <String.h>
#include <E-mail.h>
#include <Locker.h>
#include <malloc.h>
#include <ctype.h> // For isspace.

#include <MailAddon.h>
#include <mail_util.h>

using namespace Zoidberg;


class ParseFilter : public Mail::Filter
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
	: Mail::Filter(msg), name_field("From")
{
	const char *n = msg->FindString("name_field");
	if (n)
		name_field = n;
}

status_t ParseFilter::InitCheck(BString* err)
{
	return B_OK;
}

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
	// Code similar to Component::SetToRFC822, so fix it if you change this.
	//
	while ((len = Mail::readfoldedline(**data, &buf, &buflen)) >= 2)
	{
		--len; // Don't include the \n at the end of the buffer.
		
		// convert to UTF-8
		len = Mail::rfc2047_to_utf8(&buf, &buflen, len);

		// terminate
		buf[len] = 0;

		const char *delimiter = strstr(buf, ":");
		if (delimiter == NULL)
			continue;

		piece.SetTo (buf, delimiter - buf /* length of header name */);
		piece.CapitalizeEachWord(); //-------Unified case for later fetch

		delimiter++; // Skip the colon.
		while (isspace (*delimiter))
			delimiter++; // Skip over leading white space and tabs.  To do: (comments in brackets).
		headers->AddString(piece.String(),delimiter);
	}
	if (buf != NULL)
		free(buf);

	//
	// add pseudo-header THREAD, that contains the subject
	// minus stuff in []s (added by mailing lists) and
	// Re: prefixes, added by mailers when you reply.
	// This will generally be the "thread subject".
	//
	string.SetTo(headers->FindString("Subject"));
	Mail::SubjectToThread(string);
	headers->AddString("THREAD",string.String());
	
	// name
	if (headers->FindString(name_field.String(),0,&string)==B_OK)
	{
		Mail::StripGook(&string);
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

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView *view)
{
	return new ParseFilter(settings);
}

