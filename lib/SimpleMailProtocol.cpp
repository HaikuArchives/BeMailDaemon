/* SimpleProtocol - the base protocol implementation
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>
#include <Path.h>
#include <String.h>
#include <Alert.h>

#include <stdio.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT SimpleProtocol;
}
}

#include <crypt.h>

#include <MailProtocol.h>
#include <StringList.h>
#include <status.h>

#include "MessageIO.h"


namespace Zoidberg {
namespace Mail {

SimpleProtocol::SimpleProtocol(BMessage *settings, Mail::StatusView *view) :
	Mail::Protocol(settings),
	status_view(view),
	error(B_OK),
	last_message(-1) {
	}

status_t SimpleProtocol::Init() {
	SetStatusReporter(status_view);
		
	error = Open(settings->FindString("server"),settings->FindInt32("port"),settings->FindInt32("flavor"));
	if (error < B_OK)
		return error;
	
	const char *password = settings->FindString("password");
	char *passwd = get_passwd(settings,"cpasswd");
	if (passwd)
		password = passwd;

	error = Login(settings->FindString("username"),password,settings->FindInt32("auth_method"));

	delete passwd;
	
	return error;
}

SimpleProtocol::~SimpleProtocol() {
}

void SimpleProtocol::PrepareStatusWindow(StringList *manifest) {
	size_t	maildrop_size = 0;
	int32	num_messages;
	
	if (settings->FindBool("leave_mail_on_server")) {
		StringList to_dl;
		manifest->NotHere(*unique_ids,&to_dl);
		
		num_messages = to_dl.CountItems();
		
		for (int32 i = 0; i < to_dl.CountItems(); i++)
			maildrop_size += MessageSize(unique_ids->IndexOf(to_dl[i]));
	} else {
		num_messages = Messages();
		maildrop_size = MailDropSize();
	}
	
	status_view->SetMaximum(maildrop_size);
	status_view->SetTotalItems(num_messages);
}
	

status_t SimpleProtocol::GetNextNewUid
(
	BString* out_uid,
	StringList *manifest,
	time_t /*timeout*/
) {
	
	do {
		last_message++;
		
		if (last_message >= unique_ids->CountItems())
			return B_NAME_NOT_FOUND;
	} while (manifest->HasItem(unique_ids->ItemAt(last_message)));
	
	#if DEBUG
	 puts(unique_ids->ItemAt(last_message));
	#endif
		
	out_uid->SetTo(unique_ids->ItemAt(last_message));
	
	return B_OK;
}

status_t SimpleProtocol::GetMessage(
	const char* uid,
	BPositionIO** out_file, BMessage* /*out_headers*/,
	BPath* out_folder_location
) {
	int32 to_retrieve = unique_ids->IndexOf(uid);
	if (to_retrieve < 0)
		return B_NAME_NOT_FOUND;

	*out_file = new Zoidberg::Mail::MessageIO(this,*out_file,to_retrieve);
	
	if (out_folder_location != NULL)
		out_folder_location->SetTo("in");
	
	return B_OK;
}

status_t SimpleProtocol::DeleteMessage(const char* uid) {
	#if DEBUG
	 printf("ID is %d\n", (int)unique_ids->IndexOf(uid)); // What should we use for int32 instead of %d?
	#endif
	Delete(unique_ids->IndexOf(uid));
	return B_OK;
}

status_t SimpleProtocol::InitCheck(BString* /*out_message*/) {
	return error;
}

}	// namespace Mail
}	// namespace Zoidberg
