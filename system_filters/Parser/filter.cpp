#include <Message.h>
#include <String.h>
#include <E-mail.h>

#include <malloc.h>

#include <MailAddon.h>

#include <mail_util.h>
#include <Locker.h>
#include <regex.h>

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
: MailFilter(msg), name()
{
	const char *n = msg->FindString("name_field");
	if (n==NULL) n = "from";
	name = n;
}

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
	
	static regex_t *rebuf, re;
	static BLocker remakelock;
	if (rebuf==NULL && remakelock.Lock())
	{
		if (rebuf==NULL)
		{
			int err = regcomp(&re, "^( *([Rr][Ee] *: *)*\\[[^\\]]*\\])* *([Rr][Ee] *: *)*", REG_EXTENDED);
			if (err)
			{
				char errbuf[1024];
				regerror(err,&re,errbuf,sizeof(errbuf)-1);
				fprintf(stderr, "Failed to compile the regex: %s\n", errbuf);
			}
			else rebuf = &re;
		}
		remakelock.Unlock();
	}
	if (rebuf)
	{
		regmatch_t match;
		if (regexec(rebuf, string.String(), 1, &match, 0) == 0)
			// we found something
			string.Remove(match.rm_so,match.rm_eo);
		
		headers->AddString("THREAD",string.String());
	}
//	headers->PrintToStream();
	
	// name
	if (headers->FindString(name.String(),0,&string)==B_OK)
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

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView *view)
{ return new ParseFilter(settings); }
