#include <Message.h>
#include <Path.h>
#include <String.h>
#include <Alert.h>

#include <stdio.h>

class _EXPORT SimpleMailProtocol;

#include <crypt.h>

#include "MailProtocol.h"
#include "StringList.h"
#include "MessageIO.h"
#include "status.h"

inline void error_alert(const char *process, status_t error) {
	BString string;
	string << "Error while " << process << ": " << strerror(error);
	(new BAlert("error_alert",string.String(),"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go(NULL);
}

SimpleMailProtocol::SimpleMailProtocol(BMessage *settings, StatusView *view) :
	MailProtocol(settings),
	status_view(view),
	error(B_OK),
	last_message(-1) {
	}

status_t SimpleMailProtocol::Init() {
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

SimpleMailProtocol::~SimpleMailProtocol() {
}

void SimpleMailProtocol::PrepareStatusWindow(StringList *manifest) {
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
	

status_t SimpleMailProtocol::GetNextNewUid
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
	
	puts(unique_ids->ItemAt(last_message));
		
	out_uid->SetTo(unique_ids->ItemAt(last_message));
	
	return B_OK;
}

status_t SimpleMailProtocol::GetMessage(
	const char* uid,
	BPositionIO** out_file, BMessage* /*out_headers*/,
	BPath* out_folder_location
) {
	int32 to_retrieve = unique_ids->IndexOf(uid);
	if (to_retrieve < 0)
		return B_NAME_NOT_FOUND;
		
	*out_file = new MessageIO(this,*out_file,to_retrieve);
	
	if (out_folder_location != NULL)
		out_folder_location->SetTo("in");
	
	return B_OK;
}

status_t SimpleMailProtocol::DeleteMessage(const char* uid) {
	printf("ID is %d\n", (int)unique_ids->IndexOf(uid)); // What should we use for int32 instead of %d?
	Delete(unique_ids->IndexOf(uid));
	return B_OK;
}

status_t SimpleMailProtocol::InitCheck(BString* /*out_message*/) {
	return error;
}
