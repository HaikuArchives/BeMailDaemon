/* Protocol - the base class for protocol filters
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <String.h>
#include <Alert.h>
#include <Query.h>
#include <VolumeRoster.h>

#include <stdio.h>
#include <assert.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT Protocol;
}
}

#include <MailProtocol.h>
#include <StringList.h>
#include <ChainRunner.h>
#include <status.h>

using namespace Zoidberg;
using namespace Zoidberg::Mail;

namespace Zoidberg {
namespace Mail {
class DeletePass : public Mail::ChainCallback {
	public:
		DeletePass(Protocol *home);
		virtual void Callback(MDStatus result);
		
	private:
		Protocol *us;
};

class MessageDeletion : public Mail::ChainCallback {
	public:
		MessageDeletion(Protocol *home, BString *uid);
		virtual void Callback(MDStatus result);
		
	private:
		Protocol *us;
		BString *message_id;
};

}
}


inline void error_alert(const char *process, status_t error) {
	BString string;
	string << "Error while " << process << ": " << strerror(error);
	Mail::ShowAlert("error_alert",string.String(),"Ok",B_WARNING_ALERT);
}


Protocol::Protocol(BMessage* settings) : Filter(settings) {
	unique_ids = NULL;
	Protocol::settings = settings;
	
	manifest = new StringList;
	
	settings->FindPointer("chain_runner",(void **)&parent);
	parent->Chain()->MetaData()->FindFlat("manifest",manifest); //---no error checking, because if it doesn't exist, it will stay empty anyway
	
	parent->RegisterProcessCallback(new DeletePass(this));
};

Protocol::~Protocol() {
	if (unique_ids != NULL)
		delete unique_ids;
		
	delete manifest;
};


#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");
							
MDStatus Protocol::ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* /*io_entry*/,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	) {
		status_t error;
				
		if (InitCheck(NULL) < B_OK)
			return MD_NO_MORE_MESSAGES;
				
		if (unique_ids == NULL) {
			unique_ids = new StringList;
			error = UniqueIDs();
			if (error < B_OK) {
				error_alert("fetching unique ids",error);
				return MD_NO_MORE_MESSAGES;
			}
			PrepareStatusWindow(manifest);
		}
				
		error = GetNextNewUid(io_uid,manifest,0); // Added 0 for timeout. Is it correct?

		if (error < B_OK) {
			if ((error != B_TIMED_OUT) && (error != B_NAME_NOT_FOUND))
				error_alert("getting next new message",error);
			
			return MD_NO_MORE_MESSAGES;
		}
		
		error = GetMessage(io_uid->String(),io_message,io_headers,io_folder);
		if (error < B_OK) {
			for (int32 i = unique_ids->IndexOf(io_uid->String()); unique_ids->ItemAt(i) != NULL;) {
				unique_ids->RemoveItem(unique_ids->ItemAt(i));
			} //-----Remove the rest of our list: these have not yet been downloaded
			
			if (error == B_NAME_NOT_FOUND)
				return MD_NO_MORE_MESSAGES;
				
			error_alert("getting a message",error);
			return MD_DISCARD; //--Would MD_NO_MORE_MESSAGES be more appropriate?
		}
				
		if (!settings->FindBool("leave_mail_on_server"))
			parent->RegisterMessageCallback(new MessageDeletion(this,io_uid));
		
		return MD_OK;
}

void Protocol::_ReservedProtocol1() {}
void Protocol::_ReservedProtocol2() {}
void Protocol::_ReservedProtocol3() {}
void Protocol::_ReservedProtocol4() {}
void Protocol::_ReservedProtocol5() {}


//	#pragma mark -


DeletePass::DeletePass(Protocol *home) : us(home) {
	//--do nothing, and do it well
}

void DeletePass::Callback(MDStatus /*status*/) {
	if (us->settings->FindBool("delete_remote_when_local")) {
		StringList query_contents;
		BQuery fido;
		BVolume boot;
		entry_ref entry;
		BVolumeRoster().GetBootVolume(&boot);
		
		fido.SetVolume(&boot);
		fido.PushAttr("MAIL:chain");
		fido.PushInt32(us->settings->FindInt32("chain"));
		fido.PushOp(B_EQ);
		fido.Fetch();
		
		BString uid;
		while (fido.GetNextRef(&entry) == B_OK) {
			BNode(&entry).ReadAttrString("MAIL:unique_id",&uid);
			query_contents.AddItem(uid.String());
		}
		
		StringList to_delete;
		query_contents.NotHere(*(us->manifest),&to_delete);
		
		for (int32 i = 0; i < to_delete.CountItems(); i++)
			us->DeleteMessage(to_delete[i]);
		
		*(us->unique_ids) -= to_delete;
	}
	if ((us->unique_ids != NULL) && (us->InitCheck() == B_OK)) {
		BMessage *meta_data = us->parent->Chain()->MetaData();
		meta_data->RemoveName("manifest");
		meta_data->AddFlat("manifest",us->unique_ids);
	}
}


//	#pragma mark -


MessageDeletion::MessageDeletion(Protocol *home, BString *uid) :
	us(home),
	message_id(uid) {}

void MessageDeletion::Callback(MDStatus /*result*/) {
	#if DEBUG
	 printf("Deleting %s\n",message_id->String());
	#endif
	us->DeleteMessage(message_id->String());
}

