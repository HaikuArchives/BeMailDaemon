#include <UTF8.h>
#include <Message.h>

class BMimeType;

class MailComponent {
	public:
		MailComponent();
		
		void AddHeaderField(const char *key, const char *value, uint32 charset = B_ISO1_CONVERSION, char encoding = 'q', bool replace_existing = true);
		const char *HeaderField(const char *key, int32 index = 0);
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
	
	private:
		BMessage headers;
};

class PlainTextBodyComponent : public MailComponent {
	public:
		PlainTextBodyComponent(const char *text = NULL);
		
		void SetEncoding(char encoding, int32 charset);
			//------encoding: q for quoted-printable (default), b for base64 (very much not reccomended)
			//------charset: use Conversion flavor constants from UTF8.h
		
		void SetText(const char *text);
		void SetText(BDataIO *text);
		
		void AppendText(const char *text);
		
		const char *Text();
		
		virtual status_t Instantiate(BPositionIO *data, size_t length);
		virtual status_t Render(BPositionIO *render_to);
	private:
		BString text;
		
		char encoding;
		int32 charset;
};