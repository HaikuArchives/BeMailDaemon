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
#include <stdlib.h>
#include <parsedate.h>
#include <unistd.h>

#include <MailAddon.h>
#include <MailSettings.h>
#include <NodeMessage.h>
#include <ChainRunner.h>
#include <status.h>
#include <mail_util.h>

#include <MDRLanguage.h>

using namespace Zoidberg;

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
	{ "Cc",				B_MAIL_ATTR_CC,		  B_STRING_TYPE },
	{ "Date",         	B_MAIL_ATTR_WHEN,     B_TIME_TYPE   },
	{ "Reply-To",     	B_MAIL_ATTR_REPLY,    B_STRING_TYPE },
	{ "Subject",      	B_MAIL_ATTR_SUBJECT,  B_STRING_TYPE },
	{ "Priority",     	B_MAIL_ATTR_PRIORITY, B_STRING_TYPE },
	{ "Mime-Version", 	B_MAIL_ATTR_MIME,     B_STRING_TYPE },
	{ "STATUS",       	B_MAIL_ATTR_STATUS,   B_STRING_TYPE },
	{ "THREAD",       	"MAIL:thread", 		  B_STRING_TYPE }, //---Not supposed to be used for this (we add it in Parser), but why not?
	{ "NAME",       	B_MAIL_ATTR_NAME,	  B_STRING_TYPE },
	{ NULL,              NULL,                 0 }
};


class FolderFilter : public Mail::Filter
{
	BString dest_string;
	BDirectory destination;
	int32 chain_id;
	int fNumberOfFilesSaved;

  public:
	FolderFilter(BMessage*);
	virtual ~FolderFilter();
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
	fNumberOfFilesSaved = 0;
	Mail::ChainRunner *runner = NULL;
	msg->FindPointer("chain_runner", (void**)&runner);
	dest_string = runner->Chain()->MetaData()->FindString("path");
	destination = dest_string.String();
}


FolderFilter::~FolderFilter ()
{
	// Save the disk cache to the actual disk so mail data won't get lost if a
	// crash happens soon after mail has been received or sent.  Mostly put
	// here because of unexpected mail daemon activity during debugging of
	// kernel crashing software.

	if (fNumberOfFilesSaved > 0)
		sync ();
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
	time_t			dateAsTime;
	const time_t   *datePntr;
	ssize_t			dateSize;
	char			numericDateString [40];
	struct tm   	timeFields;
	BString			worker;

	BDirectory		dir;

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

	BNode node(e);

	status_t err = (*io)->Seek(0,SEEK_END);
	if (err < 0)
	{
		BString error;
		MDR_DIALECT_CHOICE (
			error << "An error occurred while saving the message " << out_headers->FindString("Subject") << " to " << path.Path() << ": " << strerror(err);
			,error << out_headers->FindString("Subject") << " のメッセージを " <<  path.Path() << "に保存中にエラーが発生しました" << strerror(err);
		)
		Mail::ShowAlert(MDR_DIALECT_CHOICE ("Folder Error","フォルダエラー"),error.String(),MDR_DIALECT_CHOICE ("OK","了解"),B_WARNING_ALERT);

		return MD_ERROR;
	}

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
			when = Mail::ParseDateWithTimeZone (buf);
			if (when == -1)
				when = time (NULL); // Use current time if it's undecodable.
			attributes.AddData(B_MAIL_ATTR_WHEN, B_TIME_TYPE, &when, sizeof(when));
			break;
		}
	}

	// add "New" status, if the status hasn't been set already
	if (attributes.FindString(B_MAIL_ATTR_STATUS,&buf) < B_OK)
		attributes.AddString(B_MAIL_ATTR_STATUS,"New");

	node << attributes;

	err = e->MoveTo(&dir);
	if (err != B_OK)
	{
		BString error;
		MDR_DIALECT_CHOICE (
			error << "An error occurred while saving the message " << out_headers->FindString("Subject") << " to " << path.Path() << ": " << strerror(err);
			,error << out_headers->FindString("Subject") << " のメッセージを " <<  path.Path() << "に保存中にエラーが発生しました" << strerror(err);
		)
		Mail::ShowAlert(MDR_DIALECT_CHOICE ("Folder Error","フォルダエラー"),error.String(),MDR_DIALECT_CHOICE ("OK","了解"),B_WARNING_ALERT);

		return MD_ERROR;
	}

	// Generate a file name for the incoming message.  See also
	// Message::RenderTo which does a similar thing for outgoing messages.

	BString name = attributes.FindString("MAIL:subject");
	Mail::SubjectToThread (name); // Extract the core subject words.
	if (name.Length() <= 0)
		name = "No Subject";
	if (name[0] == '.')
		name.Prepend ("_"); // Avoid hidden files, starting with a dot.

	// Convert the date into a year-month-day fixed digit width format, so that
	// sorting by file name will give all the messages with the same subject in
	// order of date.
	dateAsTime = 0;
	if (attributes.FindData (B_MAIL_ATTR_WHEN, B_TIME_TYPE,
		(const void **) &datePntr, &dateSize) == B_OK)
		dateAsTime = *datePntr;
	localtime_r (&dateAsTime, &timeFields);
	sprintf (numericDateString, "%04d%02d%02d%02d%02d%02d",
		timeFields.tm_year + 1900,
		timeFields.tm_mon + 1,
		timeFields.tm_mday,
		timeFields.tm_hour,
		timeFields.tm_min,
		timeFields.tm_sec);
	name << " " << numericDateString;

	worker = attributes.FindString ("MAIL:from");
	Mail::StripGook (&worker);
	name << " " << worker;

	name.Truncate(222);	// reserve space for the uniquer

	// Get rid of annoying characters which are hard to use in the shell.
	name.ReplaceAll('/','_');
	name.ReplaceAll('\'','_');
	name.ReplaceAll('"','_');

	int32 uniquer = time(NULL);
	worker = name;
	int32 tries = 20;
	while ((err = e->Rename(worker.String())) == B_FILE_EXISTS && --tries > 0) {
		srand(rand());
		uniquer += (rand() >> 16) - 16384;

		worker = name;
		worker << ' ' << uniquer;
	}
	if (err < B_OK)
		printf("could not rename mail (%s)! (should be: %s)\n",strerror(err),worker.String());

	fNumberOfFilesSaved++;
	return MD_HANDLED;
}


Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView *status)
{
	return new FolderFilter(settings);
}


BView* instantiate_config_panel(BMessage *settings, BMessage *meta_data)
{
	Mail::FileConfigView *view = new Mail::FileConfigView(
		MDR_DIALECT_CHOICE ("Destination Folder:","受信箱："),
		"path",true,"/boot/home/mail/in");
	view->SetTo(settings,meta_data);

	return view;
}

