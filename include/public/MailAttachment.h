#include <MailContainer.h>
#include <MailComponent.h>

class Base64Encoded;

class MailAttachment : public MIMEMultipartContainer {
	public:
		MailAttachment(BNode *node, bool send_attributes = true);
		MailAttachment(entry_ref *ref, bool send_attributes = true);
		
		void SetTo(BNode *node, bool send_attributes = true);
		void SetTo(entry_ref *ref, bool send_attributes = true);
		
		virtual status_t MIMEType(BMimeType *mime);
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
		
		virtual void AttachedToParent(MailComponent *parent);
		
	private:
		Base64Encoded *data, *attributes;
};

class Base64Encoded : public MailComponent {
	public:
		Base64Encoded(BDataIO *data);
		Base64Encoded(const void *data, size_t length);
		
		void SetTo(BDataIO *data);
		void SetTo(const void *data, size_t length);
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
	private:
		BMallocIO data;
};