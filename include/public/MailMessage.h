#ifndef ZOIDBERG_NUMAIL_MAIL_MESSAGE_H
#define ZOIDBERG_NUMAIL_MAIL_MESSAGE_H


#include <MailContainer.h>


class MailMessage : public MailContainer {
	public:
		MailMessage(BPositionIO *mail_file = NULL);
		virtual ~MailMessage();

		status_t InitCheck() const;

		MailMessage *ReplyMessage(bool reply_to_all,
								  bool include_attachments = false,
								  const char *quote_style = "> ");
		MailMessage *ForwardMessage(bool include_attachments = false,
									const char *quote_style = "> ");
		// These return messages with the body quoted and
		// ready to send via the appropriate channel. ReplyMessage()
		// addresses the message appropriately, but ForwardMessage()
		// leaves it unaddressed.

		const char *To();
		const char *From();
		const char *ReplyTo();
		const char *CC();
		const char *Subject();
		int Priority();

		void SetSubject(const char *to);
		void SetReplyTo(const char *to);
		void SetFrom(const char *to);
		void SetTo(const char *to);
		void SetCC(const char *to);
		void SetBCC(const char *to);
		void SetPriority(int to);

		void SendViaAccount(const char *account_name);
		void SendViaAccount(int32 chain_id);

		virtual status_t AddComponent(MailComponent *component);
		virtual status_t RemoveComponent(int32 index);

		virtual status_t ManualGetComponent(MailComponent *component, int32 index);
		virtual MailComponent *GetComponent(int32 index);
		virtual int32 CountComponents() const;

		void Attach(entry_ref *ref, bool include_attributes = true);
		bool IsComponentAttachment(int32 index);

		void SetBodyTextTo(const char *text);
		const char *BodyText();

		status_t SetBody(PlainTextBodyComponent *body);
		PlainTextBodyComponent *Body();

		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);

		status_t RenderTo(BDirectory *dir);

		status_t Send(bool send_now);

	private:
		PlainTextBodyComponent *RetrieveTextBody(MailComponent *);

		status_t _status;
		int32 _chain_id;
		char *_bcc;

		int32 _num_components;
		MailComponent *_body;
		PlainTextBodyComponent *_text_body;
};

inline status_t MailMessage::InitCheck() const
{
	return _status;
}


#endif	/* ZOIDBERG_NUMAIL_MAIL_MESSAGE_H */
