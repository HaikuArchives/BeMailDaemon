#include <View.h>

class ProtocolConfigView : public BView {
	public:
		ProtocolConfigView(bool auth_methods = false, bool flavors = false, bool user = true, bool pass = true, bool host = true, bool leave_mail_on_server = true);
		
		void SetTo(BMessage *archive);
		
		void AddFlavor(const char *label);
		void AddAuthMethod(const char *label);
		
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);
		virtual	void AttachedToWindow();
		virtual void MessageReceived(BMessage *msg);
};