/* Match Header - performs action depending on matching a header value
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Node.h>
#include <String.h>

#include <stdlib.h>
#include <stdio.h>

#include "RuleFilter.h"

using namespace Zoidberg;


//class StatusView;

RuleFilter::RuleFilter(BMessage *settings) : Mail::Filter(settings) {
	// attribute is adapted to our "capitalize-each-word-in-the-header" policy
	BString attr;
	settings->FindString("attribute",&attr);
	attr.CapitalizeEachWord();
	attribute = strdup(attr.String());

	const char *regex = NULL;	
	settings->FindString("regex",&regex);
	matcher.SetPattern(regex,true);
	
	settings->FindString("argument",&arg);
	settings->FindInt32("do_what",(long *)&do_what);
}

RuleFilter::~RuleFilter()
{
	if (attribute)
		free((void *)attribute);
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
	if (!attribute)
		return MD_OK; //----That field doesn't exist? NO match
	
	if (io_headers->FindString(attribute,&data) < B_OK) { //--Maybe the capitalization was wrong?
		BString capped(attribute);
		capped.CapitalizeEachWord(); //----Enfore capitalization
		if (io_headers->FindString(capped.String(),&data) < B_OK) //----This time it's *really* not there
			return MD_OK; //---No match
	}

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
{
	return new RuleFilter(settings);
}
