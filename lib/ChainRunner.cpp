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
#include <Locker.h>
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
#include "ErrorLogWindow.h"

using namespace Zoidberg;
using Mail::ChainRunner;

struct filter_image {
	BMessage		*settings;
	Mail::Filter	*filter;
	image_id		id;
};

BLocker list_lock("mdr_chainrunner_lock");
BList running_chains, running_chain_pointers;

ErrorLogWindow *win = NULL;

#if USE_NASTY_SYNC_THREAD_HACK
	/* There is a memory leak in gethostbyname() that causes structures it
	   allocates not to be freed if they were allocated from a BLooper. So
	   we have this awful hack to ensure that gethostbyname() is not, in
	   fact, called from a BLooper. It works fairly well, too. Hopefully,
	   the OpenBeOS net stack will rectify this problem and then I can
	   turn it off. That will be nice. */

	int32 ChainRunner::thread_sync_func(void *arg) {
		ChainRunner *us = ((ChainRunner *)(arg));
		us->Lock();
		status_t val = us->Init();
		us->Unlock();
		return val;
	}
	
	status_t ChainRunner::init_addons() {
		thread_id thread = spawn_thread(&thread_sync_func,NULL,10,this);
		Unlock();
		resume_thread(thread);
		status_t result;
		wait_for_thread(thread,&result);
		Lock();
		return result;
	}
#else
	#define init_addons Init
#endif
	
_EXPORT ChainRunner *
Mail::GetRunner(int32 chain_id, Mail::StatusWindow *status, bool selfDestruct)
{
	list_lock.Lock();
	if (running_chains.HasItem((void *)(chain_id))) {
		ChainRunner *runner = (ChainRunner *)running_chain_pointers.ItemAt(running_chains.IndexOf((void *)(chain_id)));
		list_lock.Unlock();
		return runner;
	}
	list_lock.Unlock();

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
			
			list_lock.Lock();
			for (int32 i = 0; i < running_chain_pointers.CountItems(); i++)
				((ChainRunner *)(running_chain_pointers.ItemAt(i)))->Stop();
			list_lock.Unlock();

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
	_status(status),
	_statview(NULL),
	suicide(false)
{
	static DeathFilter *filter = NULL;

	if (filter == NULL)
		be_app->AddFilter(filter = new DeathFilter);
	
	list_lock.Lock();
	if (running_chains.HasItem((void *)(_chain->ID())))
		suicide = true;
	running_chains.AddItem((void *)(_chain->ID()));
	running_chain_pointers.AddItem(this);
	list_lock.Unlock();
}


ChainRunner::~ChainRunner()
{
	//--- Delete any remaining callbacks
	for (int32 i = message_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)message_cb.ItemAt(i);
	for (int32 i = process_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)process_cb.ItemAt(i);
	for (int32 i = chain_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)chain_cb.ItemAt(i);
	
	//--- Delete any filter images
	for (int32 i = 0; i < addons.CountItems(); i++) {
		filter_image *image = (filter_image *)(addons.ItemAt(i));			
		delete image->filter;
		delete image->settings;

		unload_add_on(image->id);

		delete image;
	}

	//--- Remove ourselves from the window if we haven't been already
	if ((_status != NULL) && (_statview != NULL)) {
		_status->Lock();
		if (_statview->Window())
			_status->RemoveView(_statview);

		delete _statview;
		_status->Unlock();
	}
	
	//--- Remove ourselves from the lists
	list_lock.Lock();
	running_chains.RemoveItem((void *)(_chain->ID()));
	running_chain_pointers.RemoveItem(this);
	list_lock.Unlock();
	
	//--- And delete our chain			
	if (destroy_chain)
		delete _chain;
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
	if (suicide) {
		Quit();
		return B_NAME_IN_USE;
	}
	
	Run();

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

status_t ChainRunner::Init() {
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
			return image->id;
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
			return err;
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
	return big_err;
}

void
ChainRunner::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case 'INIT':
			if (init_addons() == B_OK)
				break;
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
			
			addons.MakeEmpty();
			
			if ((_status != NULL) && (_statview != NULL)) {
				_status->Lock();
				if (_statview->Window())
					_status->RemoveView(_statview);
				else
					delete _statview;
				
				_statview = NULL;
				_status->Unlock();
			}				
			
			/*list_lock.Lock();
			running_chains.RemoveItem((void *)(_chain->ID()));
			running_chain_pointers.RemoveItem(this);
			list_lock.Unlock();*/

			if (save_chain)
				_chain->Save();

			if (destroy_self)
				Quit();
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
	const char *glort;
	bool using_tmp = (_chain->MetaData()->FindString("path",&glort) < B_OK);
		
	BDirectory tmp(using_tmp ? "/tmp" : glort);

	for (int i = 0; i < list->CountItems(); i++) {
		uid = (*list)[i];

		char *path;
		if (using_tmp)
			path = tempnam("/tmp","mail_temp_");
		else {
			BPath pathy(glort);
			pathy.Append("Downloading");
			path = (char *)malloc(B_PATH_NAME_LENGTH);
			sprintf(path,"%s (%s: %ld)...",pathy.Path(), _chain->Name(), _chain->ID());
		}
			
		BEntry *entry = new BEntry(path);
		free(path);
		BPositionIO *file = (_chain->ChainDirection() == Mail::inbound) ? new BFile(entry, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE) : NULL;
		BPath *folder = new BPath;
		BMessage *headers = new BMessage;

		for (int32 j = 0; j < addons.CountItems(); j++) {
			struct filter_image *current = (struct filter_image *)(addons.ItemAt(j));
			
			err = current->filter->ProcessMailMessage(&file,entry,headers,folder,uid);

			if (err != B_OK)
				break;
		}

		CallCallbacksFor(message_cb, err);
		
		if (err == B_MAIL_DISCARD)
			entry->Remove();
		
		if (file != NULL)
			delete file;
			
		delete entry;
		delete headers;
		delete folder;

		if (err == B_MAIL_END_FETCH || err ==  B_MAIL_END_CHAIN)
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
	if (list->CountItems() < 1)
		return;
		
	BMessage msg('GETM');
	msg.AddFlat("messages",list);
	msg.AddInt32("bytes",bytes);
	PostMessage(&msg);
}


void
ChainRunner::GetSingleMessage(const char *uid, int32 length, BPath *into)
{
	BMessage msg('GTSM');
	msg.AddString("uid",uid);
	msg.AddInt32("bytes",length);
	msg.AddString("into",into->Path());
	PostMessage(&msg);
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
	if (win == NULL)
		win = new ErrorLogWindow(BRect(200,200,400,250),"Mail Daemon Error Log",B_TITLED_WINDOW);
	
	win->AddError(B_INFO_ALERT,error);
}
