#include <MailComponent.h>

class MIMEMultipartContainer : public MailComponent {
	public:
		MIMEMultipartContainer(const char *boundary = NULL, const char *this_is_an_MIME_message_text = NULL);
		
		void SetBoundary(const char *boundary);
		void SetThisIsAnMIMEMessageText(const char *text);
		
		void AddComponent(MailComponent *component);
		MailComponent *GetComponent(int32 index);
		int32 CountComponents() const;
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		const char *_boundary;
		const char *_MIME_message_warning;
		
		BPositionIO *_io_data;
		
		BList _components_in_raw;
		BList _components_in_code;
};