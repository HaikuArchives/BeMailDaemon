#include <MailContainer.h>

class MailMessage {
	public:
		MailMessage(BPositionIO *mail_file = NULL);
				
		const char *To();
		const char *From();
		const char *ReplyTo();
		const char *CC();
		const char *Subject();
		int Priority();
		
		void SetSubject(const char *subject);
		void SetReplyTo(const char *reply_to);
		void SetFrom(const char *from);
		void SetTo(const char *from);
		void SetCC(const char *from);
		void SetBCC(const char *from);
		void SetPriority(int priority);
		
		void SendViaAccount(const char *account_name);
		void SendViaAccount(int32 chain_id);
		
		void AddComponent(MailComponent *component);
		MailComponent *GetComponent(int32 i);
		
		void Send(bool send_now, bool delete_on_send);
	
	private:
		int32 _chain_id;
		char *_bcc;
	
		int32 _num_components;
		MailComponent *_body;
};