#ifndef ZOIDBERG_MAIL_CHAIN_RUNNER_H
#define ZOIDBERG_MAIL_CHAIN_RUNNER_H
/* ChainRunner - runs the mail inbound and outbound chains
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <MailAddon.h>
#include <MailSettings.h>
#include <Looper.h>
#include <status.h>

namespace Zoidberg {

class StringList;

namespace Mail {

class StatusWindow;
class Chain;

class ChainCallback {
	public:
		virtual void Callback(status_t result) = 0;
		// Called by the callback routines in ChainRunner
		// result is the message that ended the chain
		// (MD_HANDLED, MD_DISCARD, MD_NO_MORE_MESSAGES)
		// Obviously, for process callbacks, result is
		// gauranteed to be MD_NO_MORE_MESSAGES.
};

class ChainRunner : public BLooper {
	public:
		ChainRunner(Mail::Chain *chain, Mail::StatusWindow *status,
			bool self_destruct_when_done = true, bool save_chain_when_done = true,
			bool destruct_chain_when_done = false);
		~ChainRunner();

		//----Callback functions. Callback objects will be deleted for you.
		
		void RegisterMessageCallback(ChainCallback *callback);
		// Your callback->Callback() function will be called when
		// the current message is done being processed.
		
		void RegisterProcessCallback(ChainCallback *callback);
		// Your callback->Callback() function will be called when
		// a filter returns MD_PASS_COMPLETE and before we go back
		// to waiting for new messages, or when the fetch list
		// runs out.
		
		void RegisterChainCallback(ChainCallback *callback);
		// Your callback->Callback() function will be called when
		// a filter returns MD_ALL_PASSES_DONE and before everything
		// is unloaded and sent home.
		
		void Stop();
		void ReportProgress(int bytes, int messages, const char *message = NULL);
		void GetMessages(StringList *list, size_t bytes);
		bool QuitRequested();
		
		void ShowError(const char *error);

		Mail::Chain *Chain();
		
		//----The big, bad asynchronous RunChain() function. Pretty harmless looking, huh?
		status_t RunChain(bool asynchronous = true);
		
		void MessageReceived (BMessage *msg);
	private:
		void get_messages(StringList *list);
	
		Mail::Chain *_chain;
		BList message_cb, process_cb, chain_cb;
		
		bool destroy_self, destroy_chain, save_chain;
		
		Mail::StatusWindow *_status;
		Mail::StatusView *_statview;
		BList addons;
		
		uint32	_reserved[5];
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_CHAIN_RUNNER_H */
