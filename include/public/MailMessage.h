#include <MailContainer.h>

typedef enum {
	to,
	cc,
	bcc
} recipient_class;

class MailMessage : public HeaderedContainer {
	public:
		MailMessage();
		
		void AddRecipient(recipient_class type, const char *recipient);
		
		const char *To();
		const char *From();
		const char *ReplyTo();
		const char *CC();
		const char *Subject();
		int Priority();
		
		void SetSubject(const char *subject);
		
		void SetReplyTo(const char *reply_to);
		void SetFrom(const char *from);
		
		void SendViaAccount(const char *account_name);
		void SendViaAccount(int32 chain_id);
		
		void AddComponent(MailComponent *component);
		MailComponent *GetComponent(int32 i);
		
		void Send(bool send_now, bool delete_on_send);
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	
	private:
		int32 num_components;
		MailComponent *body;
};