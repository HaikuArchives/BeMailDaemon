#include <MailContainer.h>

class MailMessage {
	public:
		MailMessage(BPositionIO *mail_file = NULL);
		~MailMessage();
				
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
		
		const char *HeaderField(const char *key);
		void SetHeaderField(const char *field, const char *value);
		
		void SendViaAccount(const char *account_name);
		void SendViaAccount(int32 chain_id);
		
		void AddComponent(MailComponent *component);
		MailComponent *GetComponent(int32 i);
		int32 CountComponents();
		
		void Attach(entry_ref *ref, bool include_attachments = true);
		bool IsComponentAttachment(int32 i);
		
		void SetBodyTextTo(const char *text);
		const char *BodyText();
		
		void RenderTo(BFile *file);
		void RenderTo(BDirectory *dir);

		void Send(bool send_now);
	
	private:
		
	
		int32 _chain_id;
		char *_bcc;
	
		int32 _num_components;
		MailComponent *_body;
		PlainTextBodyComponent *_text_body;
		BMessage *_header_kludge_yikes;
};
