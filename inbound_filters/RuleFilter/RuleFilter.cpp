#include <Node.h>
#include <String.h>

#include <stdio.h>

#include "RuleFilter.h"

class StatusView;

_EXPORT const char *pretty_name = "Match Header";


RuleFilter::RuleFilter(BMessage *settings) : Mail::Filter(settings) {
	settings->FindString("attribute",&attribute);

	const char *regex = NULL;	
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
	const char *data;
	if (io_headers->FindString(attribute,&data) < B_OK)
		return MD_OK; //----That field doesn't exist? NO match

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

status_t descriptive_name(BMessage *settings, char *buffer) {
	const char *attribute = NULL;
	settings->FindString("attribute",&attribute);
	const char *regex = NULL;
	settings->FindString("regex",&regex);

	if (!attribute || strlen(attribute) > 15)
		return B_ERROR;
	sprintf(buffer, "Match \"%s\"", attribute);

	if (!regex)
		return B_OK;

	char reg[20];
	strncpy(reg, regex, 16);
	if (strlen(regex) > 15)
		strcpy(reg + 15, "...");

	sprintf(buffer + strlen(buffer), " against \"%s\"", reg);

	return B_OK;
}

Mail::Filter* instantiate_mailfilter(BMessage* settings,Mail::StatusView *)
{ return new RuleFilter(settings); }
