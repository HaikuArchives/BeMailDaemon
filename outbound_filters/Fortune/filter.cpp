#include <Message.h>
#include <FindDirectory.h>
#include <Entry.h>
#include <Roster.h>
#include <File.h>
#include <Path.h>
#include <String.h>
#include <stdio.h>
#include <image.h>
#include <stdlib.h>
#include <MailAddon.h>
#include "NodeMessage.h"

class FortuneFilter: public MailFilter
{
	bool enabled;
	BPath path;
	BEntry fortunefile;
	status_t status;
	
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
: MailFilter(msg), enabled(msg->FindBool("enabled")), status(B_OK)
{
	if(find_directory(B_BEOS_ETC_DIRECTORY, &path) != B_OK) {
		status = B_NAME_NOT_FOUND;
		return;
	}
	path.Append("fortunes/default");
	BEntry fortunefile(path.Path());
}

status_t FortuneFilter::InitCheck(BString* err){ return status; }

MDStatus FortuneFilter::ProcessMailMessage
(BPositionIO** io, BEntry* io_entry, BMessage* headers, BPath* , BString*)
{
	int32 ret;
	
	if (enabled && fortunefile.InitCheck() == B_OK && fortunefile.Exists())
	{
		(*io)->Seek(0, SEEK_END);

		FILE * fd;
		char buffer[768];
	
		fd = popen("/bin/fortune", "r");

		while (fgets(buffer, 768, fd)) {
			(*io)->Write(buffer, strlen(buffer));
		}		
	} 
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings)
{ return new FortuneFilter(settings); }
