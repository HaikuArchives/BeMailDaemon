/* ChainRunner - runs the mail inbound and outbound chains
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT ChainRunner;
}
}

#include "ChainRunner.h"
#include "status.h"


namespace Zoidberg {
namespace Mail {

struct async_args {
	Mail::Chain *home;
	Mail::ChainRunner *runner;
	Mail::StatusWindow *status;
	
	bool self_destruct;
	bool destruct_chain;
	bool save_chain;
};

struct filter_image {
	BMessage *settings;
	Mail::Filter *filter;
	image_id id;
};

void unload(void *id);

ChainRunner::ChainRunner(Mail::Chain *chain) :
  _chain(chain) {
	//------do absolutely nothing--------
}

ChainRunner::~ChainRunner()
{
	// clean up if the chain have not been run at all
	for (int32 i = message_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)message_cb.ItemAt(i);
	for (int32 i = process_cb.CountItems();i-- > 0;)
		delete (Mail::ChainCallback *)process_cb.ItemAt(i);
}

void ChainRunner::RegisterMessageCallback(Mail::ChainCallback *callback) {
	message_cb.AddItem(callback);
}

void ChainRunner::RegisterProcessCallback(Mail::ChainCallback *callback) {
	process_cb.AddItem(callback);
}

Mail::Chain *ChainRunner::Chain() {
	return _chain;
}

status_t ChainRunner::RunChain(Mail::StatusWindow *status,bool self_destruct_when_done, bool asynchronous, bool save_chain_when_done, bool destruct_chain_when_done) {
	BString thread_name(_chain->Name());
	thread_name += (_chain->ChainDirection() == inbound) ? "_inbound" : "_outbound";
	
	if (find_thread(thread_name.String()) >= 0)
		return B_NAME_IN_USE;
	
	struct async_args *args = new struct async_args;
	args->home = _chain;
	args->runner = this;
	args->self_destruct = self_destruct_when_done;
	args->status = status;
	args->destruct_chain = destruct_chain_when_done;
	args->save_chain = save_chain_when_done;
	
	if (asynchronous) {
		thread_id thread = spawn_thread(&async_chain_runner,thread_name.String(),10,args);
		
		if (thread < 0)
			return async_chain_runner(args);
			
		return resume_thread(thread);
	}
	
	return async_chain_runner(args);
}

int32 ChainRunner::async_chain_runner(void *arg) {
	Mail::Chain *chain;
	ChainRunner *runner;
	Mail::StatusView *status;
	Mail::StatusWindow *stat_win;
	bool self_destruct;
	bool destroy_chain;
	bool save_chain;
	status_t err = 0;
		
	{
		BString desc;
		
		struct async_args *args = (struct async_args *)(arg);
		chain = args->home;
		runner = args->runner;
		
		desc << ((chain->ChainDirection() == inbound) ? "Fetching" : "Sending") << " mail for " << chain->Name();
		stat_win = args->status;
		status = stat_win->NewStatusView(desc.String(),chain->ChainDirection() == outbound);
		self_destruct = args->self_destruct;
		destroy_chain = args->destruct_chain;
		save_chain = args->save_chain;
		delete args;
	}
	
	BList addons;
	
	BMessage settings;
	entry_ref addon;
	struct filter_image *current;
	MDStatus last_result = MD_OK;
	
	BDirectory tmp("/tmp");
	
	while (last_result != MD_NO_MORE_MESSAGES) { //------Message loop. Break with a MD_NO_MORE_MESSAGES
		char *path = tempnam("/tmp","mail_temp_");
		BPositionIO *file = new BFile(path, B_READ_WRITE | B_CREATE_FILE);
		BEntry *entry = new BEntry(path);
		free(path);
		BPath *folder = new BPath;
		BMessage *headers = new BMessage;
		BString uid = B_EMPTY_STRING;

		for (int32 i = 0; chain->GetFilter(i,&settings,&addon) >= B_OK; i++) {			
			if (addons.CountItems() <= i) { //------eek! not enough filters? load the addon
				struct filter_image *image = new struct filter_image;
				BPath path(&addon);
				Mail::Filter *(* instantiate)(BMessage *,Mail::StatusView *);
				
				image->id = load_add_on(path.Path());
				
				if (image->id < B_OK) {
					BString error;
					error << "Error loading the mail addon " << path.Path() << " from chain " << chain->Name() << ": " << strerror(image->id);
					ShowAlert("add-on error",error.String(),"Ok",B_WARNING_ALERT);

					err = -1;
					goto err;
				}
				
				status_t err = get_image_symbol(image->id,"instantiate_mailfilter",B_SYMBOL_TYPE_TEXT,(void **)&instantiate);
				on_exit_thread(&unload,(void *)(image->id));
				if (err < B_OK) {
					BString error;
					error << "Error loading the mail addon " << path.Path() << " from chain " << chain->Name() << ": the addon does not seem to be a mail addon (missing symbol instantiate_mailfilter).";
					ShowAlert("add-on error",error.String(),"Ok",B_WARNING_ALERT);

					err = -1;
					goto err;
				}
				
				image->settings = new BMessage(settings);
				
				image->settings->AddPointer("chain_runner",runner);
				image->settings->AddInt32("chain",chain->ID());
				image->filter = (*instantiate)(image->settings,status);
				
				addons.AddItem(image);
				
			}
			current = (struct filter_image *)(addons.ItemAt(i));
			
			last_result = current->filter->ProcessMailMessage(&file,entry,headers,folder,&uid);

			if (last_result != MD_OK) {
				if (last_result == MD_DISCARD)
					entry->Remove();
				
				break;
			}
		}
		
		err:
		Mail::ChainCallback *cb;
		for (int32 i = 0; i < runner->message_cb.CountItems(); i++) {
			cb = (Mail::ChainCallback *)(runner->message_cb.ItemAt(i));
			cb->Callback(last_result);
			delete cb;
		}
		
		runner->message_cb.MakeEmpty();
		
		if (status->CountTotalItems() > 0)
			status->AddItem();
		
		if (tmp.Contains(entry))
			entry->Remove();
		
		delete file;
		delete entry;
		delete headers;
		delete folder;
		if(err)
			break;
	}
	
	Mail::ChainCallback *cb;
	for (int32 i = 0; i < runner->process_cb.CountItems(); i++) {
		cb = (Mail::ChainCallback *)(runner->process_cb.ItemAt(i));
		cb->Callback(last_result);
		delete cb;
	}
	
	runner->process_cb.MakeEmpty();
	
	struct filter_image *img;
	for (int32 i = 0; i < addons.CountItems(); i++) {
		img = (struct filter_image *)(addons.ItemAt(i));
		delete img->filter;
		
		img->settings->RemoveName("chain_runner");
		img->settings->RemoveName("chain");
		chain->GetFilter(i,&settings,&addon);
		chain->SetFilter(i,*(img->settings),addon);

		delete img->settings;
		
		delete img;
	}
	
	if (status->Window() != NULL)
		stat_win->RemoveView(status);
	else
		delete status;
		
	if (self_destruct)
		delete runner;
		
	if (save_chain)
		chain->Save();
		
	if (destroy_chain)
		delete chain;
	
	return 0;
}

void unload(void *id) {
	unload_add_on((image_id)(id));
}

}	// namespace Mail
}	// namespace Zoidberg
