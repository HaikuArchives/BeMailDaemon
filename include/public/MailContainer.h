#ifndef ZOIDBERG_NUMAIL_MAILCONTAINER_H
#define ZOIDBERG_NUMAIL_MAILCONTAINER_H

class BPositionIO;

#include <List.h>

#include <MailComponent.h>


class MailContainer : public MailComponent {
	public:
		virtual status_t AddComponent(MailComponent *component) = 0;
		virtual status_t RemoveComponent(int32 index) = 0;

		virtual status_t ManualGetComponent(MailComponent *component, int32 index) = 0;
		virtual MailComponent *GetComponent(int32 index) = 0;
		virtual int32 CountComponents() const = 0;
};


class MIMEMultipartContainer : public MailContainer {
	public:
		MIMEMultipartContainer(const char *boundary = NULL, const char *this_is_an_MIME_message_text = NULL);
		MIMEMultipartContainer(MIMEMultipartContainer &copy);
		virtual ~MIMEMultipartContainer();

		void SetBoundary(const char *boundary);
		void SetThisIsAnMIMEMessageText(const char *text);

		// MailContainer
		virtual status_t AddComponent(MailComponent *component);
		virtual status_t RemoveComponent(int32 index);

		virtual status_t ManualGetComponent(MailComponent *component, int32 index);
		virtual MailComponent *GetComponent(int32 index);
		virtual int32 CountComponents() const;

		// MailComponent
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		const char *_boundary;
		const char *_MIME_message_warning;
		
		BPositionIO *_io_data;
		
		BList _components_in_raw;
		BList _components_in_code;
};

#endif
