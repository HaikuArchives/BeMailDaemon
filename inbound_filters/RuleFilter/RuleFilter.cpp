#include <Node.h>
#include <String.h>

#include <stdio.h>

#include "RuleFilter.h"

class StatusView;

RuleFilter::RuleFilter(BMessage *settings) : MailFilter(settings) {
	settings->FindString("attribute",&attribute);
	
	const char *regex;
	settings->FindString("regex",&regex);
	matcher.SetPattern(regex,true);
	
	settings->FindString("argument",&arg);
	settings->FindInt32("do_what",(long *)&do_what);
}

status_t RuleFilter::InitCheck(BString* out_message) {
	return B_OK;
}

MDStatus RuleFilter::ProcessMailMessage
(
	BPositionIO** , BEntry* entry,
	BMessage* io_headers, BPath* io_folder, BString* 
) {
	const char* data;
	io_headers->FindString(attribute,&data);
	puts("got data");
	if (data == NULL)
		return MD_OK; //----That field doesn't exist? NO match
	puts(data);
	if (!matcher.Match(data))
		return MD_OK; //-----There wasn't an error. We're just not supposed to do anything
		
	switch (do_what) {
		case Z_MOVE_TO:
			if (io_headers->ReplaceString("DESTINATION",arg) != B_OK)
				io_headers->AddString("DESTINATION",arg);
			break;
		case Z_TRASH:
			return MD_DISCARD;
		case Z_FLAG:
			{
			BString string = arg;
			BNode(entry).WriteAttrString("MAIL:filter_flags",&string);
			}
			break;
		default:
			fprintf(stderr,"Unknown do_what: 0x%04x!\n",do_what);
	}
	
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings,StatusView *)
{ return new RuleFilter(settings); }
