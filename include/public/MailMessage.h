#ifndef ZOIDBERG_NUMAIL_MAIL_MESSAGE_H
#define ZOIDBERG_NUMAIL_MAIL_MESSAGE_H


#include <MailContainer.h>

namespace Mail {

class Message : public Mail::Container {
	public:
		Message(BPositionIO *mail_file = NULL);
		virtual ~Message();

		status_t InitCheck() const;

		Message *ReplyMessage(bool reply_to_all,
							  bool include_attachments = false,
							  const char *quote_style = "> ");
		Message *ForwardMessage(bool include_attachments = false,
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

		virtual status_t AddComponent(Mail::Component *component);
		virtual status_t RemoveComponent(int32 index);

		virtual Mail::Component *GetComponent(int32 index);
		virtual int32 CountComponents() const;

		void Attach(entry_ref *ref, bool include_attributes = true);
		bool IsComponentAttachment(int32 index);

		void SetBodyTextTo(const char *text);
		const char *BodyText();

		status_t SetBody(Mail::TextComponent *body);
		Mail::TextComponent *Body();

		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);

		status_t RenderTo(BDirectory *dir);

		status_t Send(bool send_now);

	private:
		Mail::TextComponent *RetrieveTextBody(Component *);

		status_t _status;
		int32 _chain_id;
		char *_bcc;

		int32 _num_components;
		Mail::Component *_body;
		Mail::TextComponent *_text_body;
};

inline status_t Message::InitCheck() const
{
	return _status;
}

}

#endif	/* ZOIDBERG_NUMAIL_MAIL_MESSAGE_H */
