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
	printf("Constructor called.\n");
}

status_t FortuneFilter::InitCheck(BString* err){ return B_OK; }

MDStatus FortuneFilter::ProcessMailMessage
(BPositionIO** io, BEntry* io_entry, BMessage* headers, BPath* , BString*)
{
	printf ("Start your engines!\n");
	FILE * fd;
	char buffer[768];
	
	fd = popen("/bin/fortune", "r");
	if (fd) {
		printf("Looking good...\n");
		(*io)->Seek(0, SEEK_END);

		while (fgets(buffer, 768, fd)) {
			(*io)->Write(buffer, strlen(buffer));
		}
	} else {
		printf("Damnit!...\n");
	}
	printf("Returning...\n");		
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView*)
{ return new FortuneFilter(settings); }
