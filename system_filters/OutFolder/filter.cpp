/* OutFolder - scans outgoing mail in a specific folder
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Directory.h>
#include <String.h>
#include <Entry.h>
#include <NodeInfo.h>
#include <Path.h>
#include <E-mail.h>

#include <stdio.h>

#include <MailAddon.h>
#include <NodeMessage.h>
#include <ChainRunner.h>
#include <status.h>
#include <FileConfigView.h>

using namespace Zoidberg;


class StatusChanger : public Mail::ChainCallback {
	public:
		StatusChanger(entry_ref entry);
		void Callback(MDStatus result);
		
	private:
		entry_ref to_change;
};

class DiskProducer : public Mail::Filter
{
	BString src_string;
	BDirectory source;
	
	BList entries_to_send;
	Mail::ChainRunner *runner;
	bool _we_are_default_chain;
	
  public:
	DiskProducer(BMessage*,Mail::StatusView*);
	virtual status_t InitCheck(BString *err);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
};

DiskProducer::DiskProducer(BMessage* msg,Mail::StatusView*status)
	: Mail::Filter(msg)
{
	entry_ref entry;
	mail_flags flags;
	status_t result;
	int32 chain;
	BNode node;

	size_t total_size = 0;
	off_t worker;
	
	msg->FindPointer("chain_runner",(void **)&runner);
	_we_are_default_chain = (Mail::Settings().DefaultOutboundChainID() == runner->Chain()->ID());
	src_string = runner->Chain()->MetaData()->FindString("path");
 	source = src_string.String();
	while (source.GetNextRef(&entry) == B_OK) {
		node.SetTo(&entry);
		
		if (node.ReadAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&flags,4) >= B_OK
			&& flags & B_MAIL_PENDING) {
			result = node.ReadAttr("MAIL:chain",B_INT32_TYPE,0,&chain,4);
			if (((result >= B_OK) && (chain == runner->Chain()->ID())) ||
				((result < B_OK) && (_we_are_default_chain))) {
				entries_to_send.AddItem(new entry_ref(entry));
				
				node.GetSize(&worker);
				total_size += worker;
			}
		}
	}
	
	if (total_size == 0)
		return;
	
	status->SetMaximum(total_size);
	status->SetTotalItems(entries_to_send.CountItems());
}

status_t DiskProducer::InitCheck(BString* err)
{
	status_t ret = source.InitCheck();
	
	if (ret == B_OK)
		return B_OK;
	else
	{
		if (err) *err
		<< "DiskProducer failed: source directory '" << src_string
		<< "' not found (" << strerror(ret) << ").";
		return ret;
	}
}

::MDStatus DiskProducer::ProcessMailMessage(BPositionIO**io, BEntry* e, BMessage* out_headers, BPath*, BString* io_uid)
{
	if (entries_to_send.CountItems() == 0)
		return MD_NO_MORE_MESSAGES;
		
	e->Remove();
	
	entry_ref *ref = (entry_ref *)(entries_to_send.RemoveItem(0L));
		
	delete *io;
	BFile *file = new BFile(ref,B_READ_WRITE);
	
	e->SetTo(ref);
	*file >> *out_headers;
	*io = file;
	
	runner->RegisterMessageCallback(new StatusChanger(*ref));
	
	delete ref;
	
	return MD_OK;
}

StatusChanger::StatusChanger(entry_ref entry)
	: to_change(entry)
{
}

void StatusChanger::Callback(MDStatus result) {
	BNode node(&to_change);
	
	if (result == MD_HANDLED) {
		mail_flags flags = B_MAIL_SENT;
		
		node.WriteAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&flags,4);
		node.WriteAttr(B_MAIL_ATTR_STATUS,B_STRING_TYPE,0,"Sent",5);
	} else {
		node.WriteAttr(B_MAIL_ATTR_STATUS,B_STRING_TYPE,0,"Error",6);
	}
}
		

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::StatusView *status)
{
	return new DiskProducer(settings,status);
}


BView* instantiate_config_panel(BMessage *settings,BMessage *metadata)
{
	Mail::FileConfigView *view = new Mail::FileConfigView("Source Folder:","path",true,"/boot/home/mail/out");
	view->SetTo(settings,metadata);

	return view;
}

