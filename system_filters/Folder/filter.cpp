#include "ConfigView.h"

#include <Directory.h>
#include <String.h>
#include <Entry.h>
#include <NodeInfo.h>
#include <E-mail.h>
#include <Path.h>

#include <stdio.h>

#include <MailAddon.h>
#include <NodeMessage.h>

class FolderFilter: public MailFilter
{
	BString dest_string;
	BDirectory destination;
	int32 chain_id;
	
  public:
	FolderFilter(BMessage*);
	virtual status_t InitCheck(BString *err);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
};

FolderFilter::FolderFilter(BMessage* msg)
: MailFilter(msg),
  dest_string(msg->FindString("destination_path")),
  chain_id(msg->FindInt32("chain")),
  destination(dest_string.String())
{}

status_t FolderFilter::InitCheck(BString* err)
{
	status_t ret = destination.InitCheck();
	
	if (ret==B_OK) return B_OK;
	else
	{
		if (err) *err
		<< "FolderFilter failed: destination '" << dest_string
		<< "' not found (" << strerror(ret) << ").";
		return ret;
	}
}

MDStatus FolderFilter::ProcessMailMessage(BPositionIO**io, BEntry* e, BMessage* out_headers, BPath*, BString* io_uid)
{
	BDirectory dir;
	
	BPath path = dest_string.String();
	if (out_headers->HasString("destination")) {
		const char *string;
		out_headers->FindString("destination",&string);
		if (string[0] == '/')
			path = string;
		else
			path.Append(string);
	}
		
	create_directory(path.Path(),0777);
	dir.SetTo(path.Path());
	
	if (e->MoveTo(&dir) != B_OK)
	{
		// what; BAlert?
		return MD_ERROR;
	} else {
		BNode node(e);
		
		(*io)->Seek(0,SEEK_END);
		
		BNodeInfo info(&node);
		info.SetType(B_MAIL_TYPE);
		
		out_headers->AddString("MAIL:unique_id",io_uid->String());
		out_headers->AddString("MAIL:status","New");
		out_headers->AddInt32("MAIL:chain",chain_id);
		
		size_t length = (*io)->Position();
		length -= out_headers->FindInt32(B_MAIL_ATTR_HEADER);
		if (out_headers->ReplaceInt32(B_MAIL_ATTR_CONTENT,length) != B_OK)
			out_headers->AddInt32(B_MAIL_ATTR_CONTENT,length);
		
		node << *out_headers;
		
		BString name;
		name << "\"" << out_headers->FindString("MAIL:subject") << "\": <" << out_headers->FindString("MAIL:from") << ">";
		
		BString worker;
		int32 uniquer = 0;
		worker = name;
		
		while (destination.Contains(worker.String())) {
			worker = name;
			uniquer++;
			
			worker << ' ' << uniquer;
		}
		
		e->Rename(worker.String());
		
		return MD_HANDLED;
	}
}

MailFilter* instantiate_mailfilter(BMessage* settings, StatusView *status)
{
	return new FolderFilter(settings);
}

BView* instantiate_config_panel(BMessage *settings)
{
	ConfigView *view = new ConfigView();
	view->SetTo(settings);

	return view;
}
