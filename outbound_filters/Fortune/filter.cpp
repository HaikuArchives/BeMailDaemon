/* Add Fortune - adds fortunes to your mail
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <Message.h>
#include <Entry.h>
#include <String.h>
#include <stdio.h>
#include <stdlib.h>
#include <MailAddon.h>
#include "NodeMessage.h"

using namespace Zoidberg;


class FortuneFilter : public Mail::Filter
{
	BMessage *settings;
  public:
	FortuneFilter(BMessage*);
	virtual status_t InitCheck(BString *err);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
};

FortuneFilter::FortuneFilter(BMessage* msg)
	: Mail::Filter(msg), settings(msg)
{
}

status_t FortuneFilter::InitCheck(BString* err)
{
	return B_OK;
}

MDStatus FortuneFilter::ProcessMailMessage
(BPositionIO** io, BEntry* io_entry, BMessage* headers, BPath* , BString*)
{
	BString fortune_file;
	settings->FindString("fortune_file", &fortune_file);	
	fortune_file.Prepend("/bin/fortune ");

	FILE * fd;
	char buffer[768];
	BString result;
	
	fd = popen(fortune_file.String(), "r");
	if (fd) {
		(*io)->Seek(0, SEEK_END);
		
		(*io)->Write("\r\n", strlen("\r\n"));
		(*io)->Write("--\r\n",strlen("--\r\n"));
		
		result = settings->FindString("tag_line");
		result.ReplaceAll("\n","\r\n");
		(*io)->Write(result.String(), result.Length());
		result = B_EMPTY_STRING;
		
		while (fgets(buffer, 768, fd))
			result << buffer;
			
		result.ReplaceAll("\n","\r\n");
		(*io)->Write(result.String(), result.Length());
		
		pclose(fd);
	} else {
		printf("Could not open pipe to fortune!\n");
	}
	return MD_OK;
}

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView*)
{
	return new FortuneFilter(settings);
}

BView* instantiate_config_panel(BMessage *settings,BMessage *)
{
	ConfigView *view = new ConfigView();
	view->SetTo(settings);

	return view;
}
