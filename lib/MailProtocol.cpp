#include <String.h>
#include <Alert.h>
#include <Query.h>
#include <VolumeRoster.h>

#include <stdio.h>
#include <assert.h>

class _EXPORT MailProtocol;

#include "MailProtocol.h"
#include "StringList.h"
#include "ChainRunner.h"
#include "status.h"


class DeletePass : public MailCallback {
	public:
		DeletePass(MailProtocol *home);
		virtual void Callback(MDStatus result);
		
	private:
		MailProtocol *us;
};

class MessageDeletion : public MailCallback {
	public:
		MessageDeletion(MailProtocol *home, BString *uid);
		virtual void Callback(MDStatus result);
		
	private:
		MailProtocol *us;
		BString *message_id;
};

inline void error_alert(const char *process, status_t error) {
	BString string;
	string << "Error while " << process << ": " << strerror(error);
	ShowAlert("error_alert",string.String(),"Ok",B_WARNING_ALERT);
}

MailProtocol::MailProtocol(BMessage* settings) : MailFilter(settings) {
	unique_ids = NULL;
	MailProtocol::settings = settings;
	
	manifest = new StringList;
	
	settings->FindPointer("chain_runner",(void **)&parent);
	parent->Chain()->MetaData()->FindFlat("manifest",manifest); //---no error checking, because if it doesn't exist, it will stay empty anyway
	
	parent->RegisterProcessCallback(new DeletePass(this));
};

MailProtocol::~MailProtocol() {
	if (unique_ids != NULL)
		delete unique_ids;
		
	delete manifest;
};


#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");
							
MDStatus MailProtocol::ProcessMailMessage
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
			if (error == B_NAME_NOT_FOUND)
				return MD_NO_MORE_MESSAGES;
				
			error_alert("getting a message",error);
			return MD_DISCARD; //--Would MD_NO_MORE_MESSAGES be more appropriate?
		}
				
		if (!settings->FindBool("leave_mail_on_server"))
			parent->RegisterMessageCallback(new MessageDeletion(this,io_uid));
		
		return MD_OK;
}

DeletePass::DeletePass(MailProtocol *home) : us(home) {
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
	if (us->unique_ids != NULL) {
		BMessage *meta_data = us->parent->Chain()->MetaData();
		meta_data->RemoveName("manifest");
		meta_data->AddFlat("manifest",us->unique_ids);
	}
}

MessageDeletion::MessageDeletion(MailProtocol *home, BString *uid) :
	us(home),
	message_id(uid) {}

void MessageDeletion::Callback(MDStatus /*result*/) {
	#if DEBUG
	 printf("Deleting %s\n",message_id->String());
	#endif
	us->DeleteMessage(message_id->String());
}
