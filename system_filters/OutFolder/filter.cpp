/* Outbox - scans outgoing mail in a specific folder
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
#include <StringList.h>

#include <MDRLanguage.h>

using namespace Zoidberg;


class StatusChanger : public Mail::ChainCallback {
	public:
		StatusChanger(const char * entry);
		void Callback(status_t result);
		
	private:
		const char * to_change;
};

class SMTPTermination : public Mail::ChainCallback {
	public:
		SMTPTermination(Mail::ChainRunner *run) : runner(run) {}
		void Callback(status_t) { runner->Stop(); }
		
	private:
		Mail::ChainRunner *runner;
};

class DiskProducer : public Mail::Filter
{
	BString src_string;
	BDirectory source;
	
	StringList paths_to_send;
	Mail::ChainRunner *runner;
	bool _we_are_default_chain;
	status_t init;
	
  public:
	DiskProducer(BMessage*,Mail::ChainRunner*);
	virtual status_t InitCheck(BString *err);
	virtual status_t ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, const char* io_uid
	);
};

DiskProducer::DiskProducer(BMessage* msg,Mail::ChainRunner*status)
	: Mail::Filter(msg), runner(status), init(B_OK)
{
	entry_ref entry;
	mail_flags flags;
	status_t result;
	int32 chain;
	BNode node;
	BPath path;
	
	size_t total_size = 0;
	off_t worker;
	
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
				path.SetTo(&entry);
				paths_to_send += path.Path();
				
				node.GetSize(&worker);
				total_size += worker;
			}
		}
	}
	
	if (total_size == 0) {
		//runner->Stop();
		init = B_ERROR;
		return;
	}
	
	runner->RegisterProcessCallback(new SMTPTermination(runner));
	runner->GetMessages(&paths_to_send,total_size);
}

status_t DiskProducer::InitCheck(BString* err)
{
	if (init != B_OK)
		return init;
		
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

status_t DiskProducer::ProcessMailMessage(BPositionIO**io, BEntry* e, BMessage* out_headers, BPath*, const char* io_uid)
{
	e->Remove();
			
	delete *io;
	BFile *file = new BFile(io_uid,B_READ_WRITE);
	
	e->SetTo(io_uid);
	*file >> *out_headers;
	*io = file;
	
	runner->RegisterMessageCallback(new StatusChanger(io_uid));
		
	return B_OK;
}

StatusChanger::StatusChanger(const char * entry)
	: to_change(entry)
{
}

void StatusChanger::Callback(status_t result) {
	BNode node(to_change);
	
	if (result == B_OK) {
		mail_flags flags = B_MAIL_SENT;
		
		node.WriteAttr(B_MAIL_ATTR_FLAGS,B_INT32_TYPE,0,&flags,4);
		node.WriteAttr(B_MAIL_ATTR_STATUS,B_STRING_TYPE,0,"Sent",5);
	} else {
		node.WriteAttr(B_MAIL_ATTR_STATUS,B_STRING_TYPE,0,"Error",6);
	}
}
		

Mail::Filter* instantiate_mailfilter(BMessage* settings, Mail::ChainRunner *runner)
{
	return new DiskProducer(settings,runner);
}


BView* instantiate_config_panel(BMessage *settings,BMessage *metadata)
{
	Mail::FileConfigView *view = new Mail::FileConfigView(MDR_DIALECT_CHOICE ("Source Folder:","送信箱："),"path",true,"/boot/home/mail/out");
	view->SetTo(settings,metadata);

	return view;
}

