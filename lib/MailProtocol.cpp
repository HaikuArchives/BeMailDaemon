/* Protocol - the base class for protocol filters
**
** Copyright 2001-2003 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <String.h>
#include <Alert.h>
#include <Query.h>
#include <VolumeRoster.h>
#include <StringList.h>
#include <E-mail.h>
#include <NodeInfo.h>
#include <Directory.h>
#include <NodeMonitor.h>

#include <stdio.h>
#include <assert.h>

#include <MDRLanguage.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT Protocol;
}
}

#include <MailProtocol.h>
#include <ChainRunner.h>
#include <status.h>

using namespace Zoidberg;
using Zoidberg::Mail::Protocol;

namespace Zoidberg {
namespace Mail {

class ManifestAdder : public Mail::ChainCallback {
	public:
		ManifestAdder(StringList *list, const char *id) : manifest(list), uid(id) {}
		virtual void Callback(status_t result) {
			if (result == B_OK)
				(*manifest) += uid;
		}
		
	private:
		StringList *manifest;
		const char *uid;
};

class MessageDeletion : public Mail::ChainCallback {
	public:
		MessageDeletion(Protocol *home, const char *uid, BEntry *io_entry, bool delete_anyway);
		virtual void Callback(status_t result);
		
	private:
		Protocol *us;
		bool always;
		const char *message_id;
		BEntry *entry;
};

}	// namespace Mail
}	// namespace Zoidberg


inline void
Protocol::error_alert(const char *process, status_t error)
{
	BString string;
	MDR_DIALECT_CHOICE (
		string << "Error while " << process << ": " << strerror(error);
		runner->ShowError(string.String());
	,
		string << process << "中にエラーが発生しました: " << strerror(error);
		runner->ShowError(string.String());
	)
}


class DeleteHandler : public BHandler {
	public:
		DeleteHandler(Protocol *a)
			: us(a)
		{
		}

		void MessageReceived(BMessage *msg)
		{
			if ((msg->what == 'DELE') && (us->InitCheck() == B_OK)) {
				us->CheckForDeletedMessages();
				Looper()->RemoveHandler(this);
				delete this;
			}
		}

	private:
		Protocol *us;
};

class TrashMonitor : public BHandler {
	public:
		TrashMonitor(Protocol *a)
			: us(a), trash("/boot/home/Desktop/Trash")
		{
		}

		void MessageReceived(BMessage *msg)
		{
			if (msg->what == 'INIT') {
				node_ref to_watch;
				trash.GetNodeRef(&to_watch);
				watch_node(&to_watch,B_WATCH_DIRECTORY,this);
				return;
			}
			if ((msg->what == B_NODE_MONITOR) && (us->InitCheck() == B_OK) && (trash.CountEntries() == 0))
				us->CheckForDeletedMessages();
		}

	private:
		Protocol *us;
		BDirectory trash;
};

Protocol::Protocol(BMessage *settings, ChainRunner *run)
	: Filter(settings),
	runner(run), trash_monitor(NULL)
{
	unique_ids = new StringList;
	Protocol::settings = settings;

	manifest = new StringList;
	runner->Chain()->MetaData()->FindFlat("manifest", manifest); //---no error checking, because if it doesn't exist, it will stay empty anyway
	
	uids_on_disk = new StringList;
	BQuery fido;
	BVolume boot;
	entry_ref entry;
	BVolumeRoster().GetBootVolume(&boot);

	fido.SetVolume(&boot);
	fido.PushAttr("MAIL:chain");
	fido.PushInt32(settings->FindInt32("chain"));
	fido.PushOp(B_EQ);
	fido.Fetch();

	BString uid;
	while (fido.GetNextRef(&entry) == B_OK) {
		BNode(&entry).ReadAttrString("MAIL:unique_id",&uid);
		uids_on_disk->AddItem(uid.String());
	}
	
	(*manifest) |= (*uids_on_disk);
	
	if (!settings->FindBool("login_and_do_nothing_else_of_any_importance")) {
		DeleteHandler *h = new DeleteHandler(this);
		runner->AddHandler(h);
		runner->PostMessage('DELE',h);
		
		trash_monitor = new TrashMonitor(this);
		runner->AddHandler(trash_monitor);
		runner->PostMessage('INIT',trash_monitor);
		
	}
}


Protocol::~Protocol()
{
	if (manifest != NULL) {
		BMessage *meta_data = runner->Chain()->MetaData();
		meta_data->RemoveName("manifest");
		//if (settings->FindBool("leave_mail_on_server"))
			meta_data->AddFlat("manifest", manifest);
	}

	delete unique_ids;
	delete manifest;
	if (trash_monitor != NULL)
		delete trash_monitor;
}


#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < (a)->CountItems(); i++)\
								puts((a)->ItemAt(i)); \
							puts("Done\n");
							
status_t
Protocol::ProcessMailMessage(BPositionIO **io_message, BEntry *io_entry,
	BMessage *io_headers, BPath *io_folder, const char *io_uid)
{
	status_t error;

	if (io_uid == NULL)
		return B_ERROR;
	
	error = GetMessage(io_uid, io_message, io_headers, io_folder);
	if (error < B_OK) {
		if (error != B_MAIL_END_FETCH) {
			MDR_DIALECT_CHOICE (
				error_alert("getting a message",error);,
				error_alert("新しいメッセージヲ取得中にエラーが発生しました",error);
			);
		}
		return B_MAIL_END_FETCH;
	}

	runner->RegisterMessageCallback(new ManifestAdder(manifest, io_uid));
	runner->RegisterMessageCallback(new MessageDeletion(this, io_uid, io_entry, !settings->FindBool("leave_mail_on_server")));

	return B_OK;
}

void Protocol::CheckForDeletedMessages() {
	{
		//---Delete things from the manifest no longer on the server
		StringList temp;
		manifest->NotThere(*unique_ids, &temp);
		(*manifest) -= temp;
	}

	if (((settings->FindBool("delete_remote_when_local")) || !(settings->FindBool("leave_mail_on_server"))) && (manifest->CountItems() > 0)) {
		StringList to_delete;
		
		if (uids_on_disk == NULL) {
			StringList query_contents;
			BQuery fido;
			BVolume boot;
			entry_ref entry;
			BVolumeRoster().GetBootVolume(&boot);
	
			fido.SetVolume(&boot);
			fido.PushAttr("MAIL:chain");
			fido.PushInt32(settings->FindInt32("chain"));
			fido.PushOp(B_EQ);
			fido.Fetch();
	
			BString uid;
			while (fido.GetNextRef(&entry) == B_OK) {
				BNode(&entry).ReadAttrString("MAIL:unique_id",&uid);
				query_contents.AddItem(uid.String());
			}
	
			query_contents.NotHere(*manifest,&to_delete);
		} else {
			uids_on_disk->NotHere(*manifest,&to_delete);
			delete uids_on_disk;
			uids_on_disk = NULL;
		}			

		for (int32 i = 0; i < to_delete.CountItems(); i++)
			DeleteMessage(to_delete[i]);
		
		//*(unique_ids) -= to_delete; --- This line causes bad things to
		// happen (POP3 client uses the wrong indices to retrieve
		// messages).  Without it, bad things don't happen.
		*(manifest) -= to_delete;
	}
}

void Protocol::_ReservedProtocol1() {}
void Protocol::_ReservedProtocol2() {}
void Protocol::_ReservedProtocol3() {}
void Protocol::_ReservedProtocol4() {}
void Protocol::_ReservedProtocol5() {}


//	#pragma mark -


Mail::MessageDeletion::MessageDeletion(Protocol *home, const char *uid,
	BEntry *io_entry, bool delete_anyway)
	:
	us(home),
	always(delete_anyway),
	message_id(uid), entry(io_entry)
{
}


void
Mail::MessageDeletion::Callback(status_t result)
{
	#if DEBUG
	 printf("Deleting %s\n", message_id);
	#endif
	BNode node(entry);
	BNodeInfo info(&node);
	char type[255];
	info.GetType(type);
	if ((always && strcmp(B_MAIL_TYPE,type) == 0) || result == B_MAIL_DISCARD)
		us->DeleteMessage(message_id);
}

