#ifndef ZOIDBERG_MAIL_CONTAINER_H
#define ZOIDBERG_MAIL_CONTAINER_H
/* Container - message part container class
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <List.h>

#include <MailComponent.h>

class BPositionIO;


namespace Zoidberg {
namespace Mail {

class Container : public Mail::Component {
	public:
		virtual status_t AddComponent(Mail::Component *component) = 0;
		virtual status_t RemoveComponent(int32 index) = 0;

		virtual Mail::Component *GetComponent(int32 index) = 0;
		virtual int32 CountComponents() const = 0;
};

class MIMEMultipartContainer : public Mail::Container {
	public:
		MIMEMultipartContainer(const char *boundary = NULL, const char *this_is_an_MIME_message_text = NULL);
		MIMEMultipartContainer(MIMEMultipartContainer &copy);
		virtual ~MIMEMultipartContainer();

		void SetBoundary(const char *boundary);
		void SetThisIsAnMIMEMessageText(const char *text);

		// MailContainer
		virtual status_t AddComponent(Mail::Component *component);
		virtual status_t RemoveComponent(int32 index);

		virtual Mail::Component *GetComponent(int32 index);
		virtual int32 CountComponents() const;

		// MailComponent
		virtual status_t GetDecodedData(BPositionIO *data);
		virtual status_t SetDecodedData(BPositionIO *data);
		
		virtual status_t SetToRFC822(BPositionIO *data, size_t length, bool parse_now = false);
		virtual status_t RenderToRFC822(BPositionIO *render_to);
	private:
		const char *_boundary;
		const char *_MIME_message_warning;
		
		BPositionIO *_io_data;
		
		BList _components_in_raw;
		BList _components_in_code;
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_CONTAINER_H */
