/* ChainRunner - runs the mail inbound and outbound chains
**
** Copyright 2001-2003 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <List.h>
#include <OS.h>
#include <Path.h>
#include <image.h>
#include <Entry.h>
#include <File.h>
#include <String.h>
#include <ClassInfo.h>
#include <Alert.h>
#include <Directory.h>
#include <Application.h>
#include <MessageFilter.h>

#include <stdio.h>
#include <stdlib.h>

#include <MDRLanguage.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT ChainRunner;
}
}

#include <ChainRunner.h>
#include <status.h>
#include <StringList.h>

using namespace Zoidberg;
using Mail::ChainRunner;

struct filter_image {
	BMessage		*settings;
	Mail::Filter	*filter;
	image_id		id;
};

void unload(void *id);

BList running_chains, running_chain_pointers;
	
_EXPORT ChainRunner *
Mail::GetRunner(int32 chain_id, Mail::StatusWindow *status, bool selfDestruct)
{
	if (running_chains.HasItem((void *)(chain_id)))
		return (ChainRunner *)running_chain_pointers.ItemAt(running_chains.IndexOf((void *)(chain_id)));

	ChainRunner *runner = new ChainRunner(Mail::GetChain(chain_id), status,
		selfDestruct, true, selfDestruct);

	runner->RunChain();
	return runner;
}


class DeathFilter : public BMessageFilter {
	public:
		DeathFilter()
			: BMessageFilter(B_QUIT_REQUESTED)
		{
		}

		virtual	filter_result Filter(BMessage *, BHandler **)
		{
			be_app->MessageReceived(new BMessage('enda')); //---Stop new chains from starting

			for (int32 i = 0; i < running_chain_pointers.CountItems(); i++)
				((ChainRunner *)(running_chain_pointers.ItemAt(i)))->Stop();

			while(running_chains.CountItems() > 0)
				snooze(10000); // 1/100th of a second to avoid wasting CPU.

			return B_DISPATCH_MESSAGE;
		}
};


ChainRunner::ChainRunner(Mail::Chain *chain, Mail::StatusWindow *status,
	bool selfDestruct, bool saveChain, bool destructChain)
	:	BLooper(chain->Name()),
	_chain(chain),
	destroy_self(selfDestruct),
	destroy_chain(destructChain),
	save_chain(saveChain),
	_status(status)
{
	static DeathFilter *filter = NULL;

	if (filter == NULL)
		be_app->AddFilter(filter = new DeathFilter);
}


ChainRunner::~ChainRunner()
{
	// clean up if the chain have not been run at all
	/*for (int32 i = message_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)message_cb.ItemAt(i);
	for (int32 i = process_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)process_cb.ItemAt(i);
	for (int32 i = chain_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)chain_cb.ItemAt(i);*/
		
	running_chains.RemoveItem((void *)(_chain->ID()));
	running_chain_pointers.RemoveItem(this);
}


void
ChainRunner::RegisterMessageCallback(Mail::ChainCallback *callback)
{
	message_cb.AddItem(callback);
}


void
ChainRunner::RegisterProcessCallback(Mail::ChainCallback *callback)
{
	process_cb.AddItem(callback);
}


void
ChainRunner::RegisterChainCallback(Mail::ChainCallback *callback)
{
	chain_cb.AddItem(callback);
}


Mail::Chain *
ChainRunner::Chain()
{
	return _chain;
}


status_t
ChainRunner::RunChain(bool asynchronous)
{
	if (running_chains.HasItem((void *)(_chain->ID()))) {
		if (destroy_chain)
			delete _chain;
		delete this;
		return B_NAME_IN_USE;
	}

	Run();

	running_chains.AddItem((void *)(_chain->ID()));
	running_chain_pointers.AddItem(this);

	PostMessage('INIT');

	if (!asynchronous) {
		status_t result;
		wait_for_thread(Thread(),&result);
		return result;
	}

	return B_OK;
}


void 
ChainRunner::CallCallbacksFor(BList &list, status_t code)
{
	for (int32 i = 0; i < list.CountItems(); i++) {
		ChainCallback *callback = static_cast<ChainCallback *>(list.ItemAt(i));

		callback->Callback(code);
		delete callback;
	}
	list.MakeEmpty();
}


void
ChainRunner::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case 'INIT': {
			status_t big_err = B_OK;
			BString desc;
			entry_ref addon;

			MDR_DIALECT_CHOICE (
			desc << ((_chain->ChainDirection() == inbound) ? "Fetching" : "Sending") << " mail for " << _chain->Name(),
			desc << _chain->Name() << ((_chain->ChainDirection() == inbound) ? "より受信中..." : "へ送信中...")
			);

			_status->Lock();
			_statview = _status->NewStatusView(desc.String(),_chain->ChainDirection() == outbound);
			_status->Unlock();

			BMessage settings;
			for (int32 i = 0; _chain->GetFilter(i,&settings,&addon) >= B_OK; i++) {			
				struct filter_image *image = new struct filter_image;
				BPath path(&addon);
				Mail::Filter *(* instantiate)(BMessage *,Mail::ChainRunner *);

				image->id = load_add_on(path.Path());

				if (image->id < B_OK) {
					BString error;
					MDR_DIALECT_CHOICE (
						error << "Error loading the mail addon " << path.Path() << " from chain " << _chain->Name() << ": " << strerror(image->id);
						ShowError(error.String());,
						error << "メールアドオン " << path.Path() << " を " << _chain->Name() << "から読み込む際にエラーが発生しました: " << strerror(image->id);
						ShowError(error.String());
					)
					return;
				}

				status_t err = get_image_symbol(image->id,"instantiate_mailfilter",B_SYMBOL_TYPE_TEXT,(void **)&instantiate);
				if (err < B_OK) {
					BString error;
					MDR_DIALECT_CHOICE (
						error << "Error loading the mail addon " << path.Path() << " from chain " << _chain->Name()
							<< ": the addon does not seem to be a mail addon (missing symbol instantiate_mailfilter).";
						ShowError(error.String());,
						error << "メールアドオン " << path.Path() << " を " << _chain->Name() << "から読み込む際にエラーが発生しました"
 							<< ": そのアドオンはメールアドオンではないようです（instantiate_mailfilterシンボルがありません）";
						ShowError(error.String());
					)

					err = -1;
					return;
				}

				image->settings = new BMessage(settings);

				image->settings->AddInt32("chain",_chain->ID());
				image->filter = (*instantiate)(image->settings,this);
				addons.AddItem(image);

				if ((big_err = image->filter->InitCheck()) != B_OK) {
					//printf("InitCheck() failed (%s) in add-on %s\n",strerror(big_err),path.Path());
					break;
				}
			}
			if (big_err == B_OK)
				break;
		}

		case 'STOP': {
			CallCallbacksFor(chain_cb, B_OK);
				// who knows what the code was?

			BMessage settings;
			entry_ref addon;
			
			for (int32 i = 0; i < addons.CountItems(); i++) {
				filter_image *image = (filter_image *)(addons.ItemAt(i));
				delete image->filter;

				if (save_chain) {
					image->settings->RemoveName("chain");
					_chain->GetFilter(i,&settings,&addon);
					_chain->SetFilter(i,*(image->settings),addon);
				}

				delete image->settings;

				unload_add_on(image->id);

				delete image;
			}

			if (_status != NULL) {
				_status->Lock();
				if (_statview->Window())
					_status->RemoveView(_statview);
				else
					delete _statview;
				_status->Unlock();
			}				

			running_chains.RemoveItem((void *)(_chain->ID()));
			running_chain_pointers.RemoveItem(this);

			if (save_chain)
				_chain->Save();

			if (destroy_chain)
				delete _chain;

			if (destroy_self) {
				Quit();
				delete this;
			}
			break;
		}
		case 'GETM': {
			StringList list;
			msg->FindFlat("messages",&list);
			_statview->SetTotalItems(list.CountItems());
			_statview->SetMaximum(msg->FindInt32("bytes"));
			get_messages(&list); }
			break;
		case 'GTSM': {
			const char *uid;
			status_t err = B_OK;
			msg->FindString("uid",&uid);
			BEntry *entry = new BEntry(msg->FindString("into"));
			_statview->SetTotalItems(1);
			_statview->SetMaximum(msg->FindInt32("bytes"));
			BPositionIO *file = new BFile(entry, B_READ_WRITE | B_CREATE_FILE);
			BPath *folder = new BPath;
			BMessage *headers = new BMessage;
			headers->AddBool("ENTIRE_MESSAGE",true);

			for (int32 j = 0; j < addons.CountItems(); j++) {
				struct filter_image *current = (struct filter_image *)(addons.ItemAt(j));

				err = current->filter->ProcessMailMessage(&file,entry,headers,folder,uid);
				if (err != B_OK)
					break;
			}

			CallCallbacksFor(message_cb, err);

			delete file;
			delete entry;
			delete headers;
			delete folder;

			CallCallbacksFor(process_cb, err);

			if (save_chain) {
				entry_ref addon;
				BMessage settings;
				for (int32 i = 0; i < addons.CountItems(); i++) {
					filter_image *image = (filter_image *)(addons.ItemAt(i));

					BMessage *temp = new BMessage(*(image->settings));
					temp->RemoveName("chain");
					_chain->GetFilter(i, &settings, &addon);
					_chain->SetFilter(i, *temp, addon);

					delete temp;
				}

				_chain->Save();
			}

			ResetProgress();
			break;
		}
	}
}


bool
ChainRunner::QuitRequested()
{
	Stop();
	return true;
}


void
ChainRunner::get_messages(StringList *list)
{
	const char *uid;

	status_t err = B_OK;
	BDirectory tmp("/tmp");

	for (int i = 0; i < list->CountItems(); i++) {
		uid = (*list)[i];

		char *path = tempnam("/tmp","mail_temp_");
		BEntry *entry = new BEntry(path);
		free(path);
		BPositionIO *file = new BFile(entry, B_READ_WRITE | B_CREATE_FILE);
		BPath *folder = new BPath;
		BMessage *headers = new BMessage;

		for (int32 j = 0; j < addons.CountItems(); j++) {
			struct filter_image *current = (struct filter_image *)(addons.ItemAt(j));
			
			err = current->filter->ProcessMailMessage(&file,entry,headers,folder,uid);
			if (err != B_OK)
				break;
		}

		CallCallbacksFor(message_cb, err);

		if (tmp.Contains(entry))
			entry->Remove();

		delete file;
		delete entry;
		delete headers;
		delete folder;

		if (err == B_MAIL_END_FETCH)
			break;
	}

	CallCallbacksFor(process_cb, err);

	if (save_chain) {
		entry_ref addon;
		BMessage settings;
		for (int32 i = 0; i < addons.CountItems(); i++) {
			filter_image *image = (filter_image *)(addons.ItemAt(i));

			BMessage *temp = new BMessage(*(image->settings));
			temp->RemoveName("chain");
			_chain->GetFilter(i,&settings,&addon);
			_chain->SetFilter(i,*temp,addon);

			delete temp;
		}

		_chain->Save();
	}

	ResetProgress();

	if (err == B_MAIL_END_CHAIN)
		Stop();
}


void
ChainRunner::Stop()
{
	PostMessage('STOP');
}


void
ChainRunner::GetMessages(StringList *list, int32 bytes)
{
	BMessage *msg = new BMessage('GETM');
	msg->AddFlat("messages",list);
	msg->AddInt32("bytes",bytes);
	PostMessage(msg);
}


void
ChainRunner::GetSingleMessage(const char *uid, int32 length, BPath *into)
{
	BMessage *msg = new BMessage('GTSM');
	msg->AddString("uid",uid);
	msg->AddInt32("bytes",length);
	msg->AddString("into",into->Path());
	PostMessage(msg);
}


void
ChainRunner::ReportProgress(int bytes, int messages, const char *message)
{
	if (bytes != 0)
		_statview->AddProgress(bytes);

	for (int i = 0; i < messages; i++)
		_statview->AddItem();	

	if (message != NULL)
		_statview->SetMessage(message);
}


void
ChainRunner::ResetProgress(const char *message)
{
	_statview->Reset();
	if (message != NULL)
		_statview->SetMessage(message);
}


void
ChainRunner::ShowError(const char *error)
{
	(new BAlert("error", error, "OK"))->Go();
}
