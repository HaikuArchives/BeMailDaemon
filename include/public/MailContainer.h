#include <MailComponent.h>

class HeaderedContainer : public MailHeaderComponent {
	public:
		HeaderedContainer(MailComponent *body = NULL);
		
		void SetBody(MailComponent *body);
		MailComponent *Body();
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
	
		virtual status_t MIMEType(BMimeType *mime);
	private:
		MailComponent *body_part;
};
	

class MIMEMultipartContainer : public MailComponent {
	public:
		MIMEMultipartContainer(const char *boundary = NULL, const char *this_is_an_MIME_message_text = NULL);
		
		void SetBoundary(const char *boundary);
		void SetThisIsAnMIMEMessageText(const char *text);
		
		void AddComponent(MailComponent *component);
		MailComponent *GetComponent(int32 index);
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	private:
		const char *boundary;
		const char *MIME_message_warning;
		
		BList components;
};