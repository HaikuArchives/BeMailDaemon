#include <List.h>
#include <OS.h>
#include <Path.h>
#include <image.h>
#include <Entry.h>
#include <File.h>
#include <String.h>
#include <ClassInfo.h>
#include <Alert.h>

#include <stdio.h>

class _EXPORT ChainRunner;

#include "ChainRunner.h"
#include "status.h"

struct async_args {
	MailChain *home;
	ChainRunner *runner;
	StatusWindow *status;
	
	bool self_destruct;
};

struct filter_image {
	BMessage *settings;
	MailFilter *filter;
	image_id id;
};

void unload(void *id);

ChainRunner::ChainRunner(MailChain *chain) :
  _chain(chain) {
	//------do absolutely nothing--------
}

ChainRunner::~ChainRunner()
{
	// clean up if the chain have not been run at all
	for (int32 i = message_cb.CountItems();i-- > 0;)
		delete (MailCallback *)message_cb.ItemAt(i);
	for (int32 i = process_cb.CountItems();i-- > 0;)
		delete (MailCallback *)process_cb.ItemAt(i);
}

void ChainRunner::RegisterMessageCallback(MailCallback *callback) {
	message_cb.AddItem(callback);
}

void ChainRunner::RegisterProcessCallback(MailCallback *callback) {
	process_cb.AddItem(callback);
}

MailChain *ChainRunner::Chain() {
	return _chain;
}

status_t ChainRunner::RunChain(StatusWindow *status,bool self_destruct_when_done, bool asynchronous) {
	BString thread_name(_chain->Name());
	thread_name += (_chain->ChainDirection() == inbound) ? "_inbound" : "_outbound";
	
	if (find_thread(thread_name.String()) >= 0)
		return B_NAME_IN_USE;
	
	struct async_args *args = new struct async_args;
	args->home = _chain;
	args->runner = this;
	args->self_destruct = self_destruct_when_done;
	args->status = status;
	
	if (asynchronous) {
		thread_id thread = spawn_thread(&async_chain_runner,thread_name.String(),10,args);
		
		if (thread < 0)
			return async_chain_runner(args);
			
		return resume_thread(thread);
	}
	
	return async_chain_runner(args);
}

int32 ChainRunner::async_chain_runner(void *arg) {
	MailChain *chain;
	ChainRunner *runner;
	StatusView *status;
	bool self_destruct;
	
	{
		BString desc;
		
		struct async_args *args = (struct async_args *)(arg);
		chain = args->home;
		runner = args->runner;
		
		desc << ((chain->ChainDirection() == inbound) ? "Fetching" : "Sending") << " mail for " << chain->Name();
		status = args->status->NewStatusView(desc.String(),chain->ChainDirection() == outbound);
		self_destruct = args->self_destruct;
		delete args;
	}
	
	BList addons;
	
	BMessage settings;
	entry_ref addon;
	struct filter_image *current;
	MDStatus last_result = MD_OK;
	
	while (last_result != MD_NO_MORE_MESSAGES) { //------Message loop. Break with a MD_NO_MORE_MESSAGES
		char *path = tempnam("/tmp","mail_temp_"); // do we need to free() tempnam()'s value?
		BPositionIO *file = new BFile(path, B_READ_WRITE | B_CREATE_FILE);
		BEntry *entry = new BEntry(path);
		BPath *folder = new BPath;
		BMessage *headers = new BMessage;
		BString uid = B_EMPTY_STRING;

		for (int32 i = 0; chain->GetFilter(i,&settings,&addon) >= B_OK; i++) {			
			if (addons.CountItems() <= i) { //------eek! not enough filters? load the addon
				struct filter_image *image = new struct filter_image;
				BPath path(&addon);
				MailFilter *(* instantiate)(BMessage *,StatusView *);
				
				image->id = load_add_on(path.Path());
				puts(path.Path());
				
				if (image->id < B_OK) {
					BString error;
					error << "Error loading the mail addon " << path.Path() << " from chain " << chain->Name() << ": " << strerror(image->id);
					
					(new BAlert("add-on error",error.String(),"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go(NULL);
					return -1;
				}
				
				status_t err = get_image_symbol(image->id,"instantiate_mailfilter",B_SYMBOL_TYPE_TEXT,(void **)&instantiate);
				on_exit_thread(&unload,(void *)(image->id));
				if (err < B_OK) {
					BString error;
					error << "Error loading the mail addon " << path.Path() << " from chain " << chain->Name() << ": the addon does not seem to be a mail addon (missing symbol instantiate_mailfilter).";
					
					(new BAlert("add-on error",error.String(),"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go(NULL);
					return -1;
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
		
		MailCallback *cb;
		for (int32 i = 0; i < runner->message_cb.CountItems(); i++) {
			cb = (MailCallback *)(runner->message_cb.ItemAt(i));
			cb->Callback(last_result);
			delete cb;
		}
		
		runner->message_cb.MakeEmpty();
		
		status->AddItem();
		
		delete file;
		delete entry;
		delete headers;
		delete folder;
	}
	
	MailCallback *cb;
	for (int32 i = 0; i < runner->process_cb.CountItems(); i++) {
		cb = (MailCallback *)(runner->process_cb.ItemAt(i));
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
	
	((StatusWindow *)(status->Window()))->RemoveView(status);
	if (self_destruct)
		delete runner;
	
	return 0;
}

void unload(void *id) {
	unload_add_on((image_id)(id));
}
