/* Protocol - the base class for protocol filters
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <String.h>
#include <Alert.h>
#include <Query.h>
#include <VolumeRoster.h>
#include <StringList.h>

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

class RanYetReset : public Mail::ChainCallback {
	public:
		RanYetReset(bool *ranyet) : _ranyet(ranyet) {}
		virtual void Callback(status_t) {
			*_ranyet = false;
		}
	private:
		bool *_ranyet;
};

class MessageDeletion : public Mail::ChainCallback {
	public:
		MessageDeletion(Protocol *home, const char *uid, bool delete_anyway);
		virtual void Callback(status_t result);
		
	private:
		Protocol *us;
		bool always;
		const char *message_id;
};

}
}


inline void Protocol::error_alert(const char *process, status_t error) {
	BString string;
	MDR_DIALECT_CHOICE (
		string << "Error while " << process << ": " << strerror(error);
		runner->ShowError(string.String());
	,
		string << process << "中にエラーが発生しました: " << strerror(error);
		runner->ShowError(string.String());
	)
}


Protocol::Protocol(BMessage* settings, ChainRunner *run) : Filter(settings), runner(run), ran_yet(false) {
	unique_ids = new StringList;
	Protocol::settings = settings;
	
	manifest = new StringList;
	runner->Chain()->MetaData()->FindFlat("manifest",manifest); //---no error checking, because if it doesn't exist, it will stay empty anyway
}

Protocol::~Protocol() {
	if (manifest != NULL) {
		BMessage *meta_data = runner->Chain()->MetaData();
		meta_data->RemoveName("manifest");
		//if (settings->FindBool("leave_mail_on_server"))
			meta_data->AddFlat("manifest",manifest);
	}
	delete unique_ids;
	if (manifest != NULL)
		delete manifest;
};


#define dump_stringlist(a) printf("StringList %s:\n",#a); \
							for (int32 i = 0; i < a->CountItems(); i++)\
								puts(a->ItemAt(i)); \
							puts("Done\n");
							
status_t Protocol::ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* /*io_entry*/,
		BMessage* io_headers, BPath* io_folder, const char* io_uid
	) {
		status_t error;
		
		if (!ran_yet) {
			if (settings->FindBool("delete_remote_when_local")) {
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
				
				StringList to_delete;
				query_contents.NotHere(*manifest,&to_delete);
				
				for (int32 i = 0; i < to_delete.CountItems(); i++)
					DeleteMessage(to_delete[i]);
				
				*(unique_ids) -= to_delete;
			}
			
			ran_yet = true;
			runner->RegisterProcessCallback(new RanYetReset(&ran_yet));
		}
		
		error = GetMessage(io_uid,io_message,io_headers,io_folder);
		if (error < B_OK) {
			MDR_DIALECT_CHOICE (
				error_alert("getting a message",error);,
				error_alert("新しいメッセージヲ取得中にエラーが発生しました",error);
			)
			return B_MAIL_END_FETCH;
		}
		
		runner->RegisterMessageCallback(new ManifestAdder(manifest,io_uid));
		runner->RegisterMessageCallback(new MessageDeletion(this,io_uid,!settings->FindBool("leave_mail_on_server")));
		
		return B_OK;
}

void Protocol::_ReservedProtocol1() {}
void Protocol::_ReservedProtocol2() {}
void Protocol::_ReservedProtocol3() {}
void Protocol::_ReservedProtocol4() {}
void Protocol::_ReservedProtocol5() {}


//	#pragma mark -


Mail::MessageDeletion::MessageDeletion(Protocol *home, const char *uid, bool delete_anyway) :
	us(home),
	always(delete_anyway),
	message_id(uid) {}

void Mail::MessageDeletion::Callback(status_t result) {
	#if DEBUG
	 printf("Deleting %s\n",message_id->String());
	#endif
	if (always || result == B_MAIL_DISCARD)
		us->DeleteMessage(message_id);
}

