#include "ConfigView.h"

#include <Message.h>
#include <Entry.h>
#include <String.h>
#include <stdio.h>
#include <stdlib.h>
#include <MailAddon.h>
#include "NodeMessage.h"

class FortuneFilter: public MailFilter
{
	bool enabled;
	BString fortune_file;	
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
: MailFilter(msg), enabled(msg->FindBool("enabled"))
{
	msg->FindString("fortune_file", &fortune_file);	
}

status_t FortuneFilter::InitCheck(BString* err){ return B_OK; }

MDStatus FortuneFilter::ProcessMailMessage
(BPositionIO** io, BEntry* io_entry, BMessage* headers, BPath* , BString*)
{
	fortune_file.Prepend("/bin/fortune ");

	printf("* Fortune * Line used: %s\n", fortune_file.String());

	FILE * fd;
	char buffer[768];
	
	fd = popen(fortune_file.String(), "r");
	if (fd) {
		(*io)->Seek(0, SEEK_END);
		
		(*io)->Write("\n", strlen("\n"));	
	
		while (fgets(buffer, 768, fd)) {
			(*io)->Write(buffer, strlen(buffer));
		}
	} else {
		printf("Damnit!\n");
	}
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView*)
{ return new FortuneFilter(settings); }


BView* instantiate_config_panel(BMessage *settings)
{
	ConfigView *view = new ConfigView();
	view->SetTo(settings);

	return view;
}
