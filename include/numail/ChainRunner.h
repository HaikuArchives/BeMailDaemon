#ifndef MAIL_CHAINRUNNER_H
#define MAIL_CHAINRUNNER_H

#include <MailAddon.h>
#include <MailSettings.h>

namespace Mail {
class StatusWindow;
class Chain;

class ChainCallback {
	public:
		virtual void Callback(MDStatus result) = 0;
		// Called by the callback routines in ChainRunner
		// result is the message that ended the chain
		// (MD_HANDLED, MD_DISCARD, MD_NO_MORE_MESSAGES)
		// Obviously, for process callbacks, result is
		// gauranteed to be MD_NO_MORE_MESSAGES.
};

class ChainRunner {
	public:
		ChainRunner(Mail::Chain *chain);
		~ChainRunner();

		//----Callback functions. Callback objects will be deleted for you.
		
		void RegisterMessageCallback(ChainCallback *callback);
		// Your callback->Callback() function will be called when
		// the current message is done being processed.
		
		void RegisterProcessCallback(ChainCallback *callback);
		// Your callback->Callback() function will be called when
		// a filter returns MD_NO_MORE_MESSAGES and before everything
		// is unloaded and sent home.
		
		Mail::Chain *Chain();
		
		//----The big, bad asynchronous RunChain() function. Pretty harmless looking, huh?
		status_t RunChain(StatusWindow *status,
			bool self_destruct_when_done,
			bool asynchronous = true,
			bool save_chain_when_done = true,
			bool destruct_chain_when_done = false);
		
	private:
		static int32 async_chain_runner(void *arg);
	
		Mail::Chain *_chain;
		BList message_cb, process_cb;
};

}

#endif //MAIL_CHAINRUNNER_H