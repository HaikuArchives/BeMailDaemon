class MailComponent;
class BMimeType;

_IMPEXP_MAIL MailComponent *instantiate_component(const char *disposition, 
												  MailComponent *parent = NULL);

class MailComponent {
	public:
		virtual status_t Instantiate(BDataIO *data, size_t length) = 0;
		virtual status_t Render(BDataIO *render_to) = 0;
		
		virtual status_t MIMEType(BMimeType *mime) = 0;
		
		virtual void AttachedToParent(MailComponent *parent);
		
		MailComponent *Parent();
};

class MailHeaderComponent : public MailComponent {
	public:
		MailHeaderComponent();
		
		void AddHeaderField(const char *key, const char *value);
		const char *HeaderField(const char *key);
		//--------All conversion for charsets and encodings is done for you
		
		void SetEncoding(char encoding, int charset);
			//------encoding: q for quoted-printable (default), b for base64 (very much not reccomended)
			//------charset: use Conversion flavor constants from UTF8.h
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
		
		virtual status_t MIMEType(BMimeType *mime);
		
	private:
		BMessage header;
};

class PlainTextBodyComponent : public MailComponent {
	public:
		PlainTextBodyComponent(const char *text = NULL);
		
		void SetEncoding(char encoding, int charset);
			//------encoding: q for quoted-printable (default), b for base64 (very much not reccomended)
			//------charset: use Conversion flavor constants from UTF8.h
		
		void SetText(const char *text);
		void SetText(BDataIO *text);
		
		void AppendText(const char *text);
		
		const char *Text();
		
		virtual status_t Instantiate(BDataIO *data, size_t length);
		virtual status_t Render(BDataIO *render_to);
		virtual void AttachedToParent(MailComponent *parent);
		
		virtual status_t MIMEType(BMimeType *mime);
	private:
		BString text;
}