/* Inbox - places the incoming mail to their destination folder
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <FileConfigView.h>

#include <Directory.h>
#include <String.h>
#include <Entry.h>
#include <NodeInfo.h>
#include <E-mail.h>
#include <Path.h>

#include <stdio.h>
#include <parsedate.h>

#include <MailAddon.h>
#include <MailSettings.h>
#include <NodeMessage.h>
#include <ChainRunner.h>
#include <status.h>

using namespace Zoidberg;

_EXPORT const char *pretty_name = "Incoming Mail Folder";


struct mail_header_field
{
	const char *rfc_name;
	
	const char *attr_name;
	type_code attr_type;
	// currently either B_STRING_TYPE and B_TIME_TYPE
};

static const mail_header_field gDefaultFields[] =
{
	{ "To",            	B_MAIL_ATTR_TO,       B_STRING_TYPE },
	{ "From",         	B_MAIL_ATTR_FROM,     B_STRING_TYPE },
	{ "Date",         	B_MAIL_ATTR_WHEN,     B_TIME_TYPE },
	{ "Reply-To",     	B_MAIL_ATTR_REPLY,    B_STRING_TYPE },
	{ "Subject",      	B_MAIL_ATTR_SUBJECT,  B_STRING_TYPE },
	{ "Priority",     	B_MAIL_ATTR_PRIORITY, B_STRING_TYPE },
	{ "Mime-Version", 	B_MAIL_ATTR_MIME,     B_STRING_TYPE },
	{ "Status",       	B_MAIL_ATTR_STATUS,   B_STRING_TYPE },
	{ "THREAD",       	"MAIL:thread", 		  B_STRING_TYPE }, //---Not supposed to be used for this (we add it in Parser), but why not?
	{ "NAME",       	B_MAIL_ATTR_NAME,	  B_STRING_TYPE },
	{ NULL,              NULL,                 0 }
};


class FolderFilter : public Mail::Filter
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
	: Mail::Filter(msg),
	chain_id(msg->FindInt32("chain"))
{
	Mail::ChainRunner *runner = NULL;
	msg->FindPointer("chain_runner", (void**)&runner);
	dest_string = runner->Chain()->MetaData()->FindString("path");
	destination = dest_string.String();
}

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
	if (out_headers->HasString("DESTINATION")) {
		const char *string;
		out_headers->FindString("DESTINATION",&string);
		if (string[0] == '/')
			path = string;
		else
			path.Append(string);
	}
		
	create_directory(path.Path(),0777);
	dir.SetTo(path.Path());
	
	status_t err = e->MoveTo(&dir);
	if (err != B_OK)
	{
		BString error;
		error << "An error occurred while saving the message " << out_headers->FindString("Subject") << " to " << path.Path() << ": " << strerror(err);
		
		Mail::ShowAlert("Folder Error",error.String(),"OK",B_WARNING_ALERT);
		
		return MD_ERROR;
	}

	BNode node(e);
	
	(*io)->Seek(0,SEEK_END);
	
	BNodeInfo info(&node);
	info.SetType(B_MAIL_TYPE);
	
	BMessage attributes;
	
	attributes.AddString("MAIL:unique_id",io_uid->String());
	attributes.AddString("MAIL:account",Mail::Chain(chain_id).Name());
	attributes.AddInt32("MAIL:chain",chain_id);
	
	size_t length = (*io)->Position();
	length -= out_headers->FindInt32(B_MAIL_ATTR_HEADER);
	if (attributes.ReplaceInt32(B_MAIL_ATTR_CONTENT,length) != B_OK)
		attributes.AddInt32(B_MAIL_ATTR_CONTENT,length);
	
	const char *buf;
	time_t when;
	for (int i = 0; gDefaultFields[i].rfc_name; ++i)
	{
		out_headers->FindString(gDefaultFields[i].rfc_name,&buf);
		if (buf == NULL)
			continue;
		
		switch (gDefaultFields[i].attr_type){
		case B_STRING_TYPE:
			attributes.AddString(gDefaultFields[i].attr_name, buf);
			break;
		
		case B_TIME_TYPE:
			when = parsedate(buf, time((time_t *)NULL));
			if (when == -1) when = time((time_t *)NULL);
			attributes.AddData(B_MAIL_ATTR_WHEN, B_TIME_TYPE, &when, sizeof(when));
			break;
		}
	}

	// add "New" status, if the status hasn't been set already
	if (attributes.FindString(B_MAIL_ATTR_STATUS,&buf) < B_OK)
		attributes.AddString(B_MAIL_ATTR_STATUS,"New");
	
	node << attributes;
	
	BString name = attributes.FindString("MAIL:subject");
	
	name << ": <" << attributes.FindString("MAIL:from");
	name.Truncate(222);
	name << ">";
	name.ReplaceAll('/', '_');
	
	BString worker;
	int32 uniquer = time(NULL);
	worker << name << uniquer;

	while (destination.Contains(worker.String())) {
		worker = name;
		uniquer++;
		
		worker << ' ' << uniquer;
	}
	
	err = e->Rename(worker.String());
	if (err < B_OK)
		printf("could not rename mail (%s)! (should be: %s)\n",strerror(err),worker.String());

	return MD_HANDLED;
}


Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView *status)
{
	return new FolderFilter(settings);
}


BView* instantiate_config_panel(BMessage *settings, BMessage *meta_data)
{
	Mail::FileConfigView *view = new Mail::FileConfigView("Destination Folder:","path",true,"/boot/home/mail/in");
	view->SetTo(settings,meta_data);

	return view;
}

