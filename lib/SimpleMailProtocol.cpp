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
#include <ChainRunner.h>
#include <status.h>

#include "MessageIO.h"


namespace Zoidberg {
namespace Mail {

class SinglePassTermination : public Mail::ChainCallback {
	public:
		SinglePassTermination(Mail::ChainRunner *run) : runner(run) {}
		void Callback(status_t) { runner->Stop(); }
		
	private:
		Mail::ChainRunner *runner;
};

SimpleProtocol::SimpleProtocol(BMessage *settings, Mail::ChainRunner *run) :
	Mail::Protocol(settings,run),
	error(B_OK),
	last_message(-1)
{
	runner->RegisterProcessCallback(new SinglePassTermination(runner));
}


status_t
SimpleProtocol::Init()
{
	error = Open(settings->FindString("server"), settings->FindInt32("port"),
				settings->FindInt32("flavor"));
	if (error < B_OK) {
		runner->Stop();
		return error;
	}

	const char *password = settings->FindString("password");
	char *passwd = get_passwd(settings, "cpasswd");
	if (passwd)
		password = passwd;

	error = Login(settings->FindString("username"), password, settings->FindInt32("auth_method"));
	delete passwd;

	if (error < B_OK) {
		runner->Stop();
		return error;
	}

	if (settings->FindBool("login_and_do_nothing_else_of_any_importance"))
		return error;

	error = UniqueIDs();
	if (error < B_OK) {
		runner->Stop();
		return error;
	}

	size_t	maildrop_size = 0;
	int32	num_messages;

	StringList to_dl;
	manifest->NotHere(*unique_ids, &to_dl);

	num_messages = to_dl.CountItems();
	if (num_messages == 0) {
		runner->Stop();
		return error;
	}

	for (int32 i = 0; i < to_dl.CountItems(); i++)
		maildrop_size += MessageSize(unique_ids->IndexOf(to_dl[i]));

	runner->GetMessages(&to_dl, maildrop_size);

	return error;
}


SimpleProtocol::~SimpleProtocol()
{
}
	

status_t
SimpleProtocol::GetMessage(const char *uid, BPositionIO **out_file, BMessage *out_headers,
	BPath *out_folder_location)
{
	int32 to_retrieve = unique_ids->IndexOf(uid);
	if (to_retrieve < 0)
		return B_NAME_NOT_FOUND;

	out_headers->AddInt32("SIZE",MessageSize(to_retrieve));
	*out_file = new Zoidberg::Mail::MessageIO(this,*out_file,to_retrieve);

	if (out_folder_location != NULL)
		out_folder_location->SetTo("");

	return B_OK;
}


status_t
SimpleProtocol::DeleteMessage(const char *uid)
{
#if DEBUG
	printf("ID is %d\n", (int)unique_ids->IndexOf(uid)); // What should we use for int32 instead of %d?
#endif
	Delete(unique_ids->IndexOf(uid));
	return B_OK;
}


status_t
SimpleProtocol::InitCheck(BString* /*out_message*/)
{
	return error;
}

}	// namespace Mail
}	// namespace Zoidberg
