#ifndef ZOIDBERG_NUMAIL_MAIL_MESSAGE_H
#define ZOIDBERG_NUMAIL_MAIL_MESSAGE_H


//#include <SupportDefs.h>

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
		
		status_t SetBody(PlainTextBodyComponent *body);
		PlainTextBodyComponent *Body() const;

		status_t RenderTo(BFile *file);
		status_t RenderTo(BDirectory *dir);

		status_t Send(bool send_now);
	
	private:
		int32 _chain_id;
		char *_bcc;
	
		int32 _num_components;
		MailComponent *_body;
		PlainTextBodyComponent *_text_body;
		BMessage *_header_kludge_yikes;
};

inline PlainTextBodyComponent *MailMessage::Body() const
{
	return _text_body;
}

#endif	/* ZOIDBERG_NUMAIL_MAIL_MESSAGE_H */
