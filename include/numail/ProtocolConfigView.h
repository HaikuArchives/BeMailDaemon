#include <View.h>

typedef enum {
	Z_HAS_AUTH_METHODS 			= 1,
	Z_HAS_FLAVORS 				= 2,
	Z_HAS_USERNAME 				= 4,
	Z_HAS_PASSWORD 				= 8,
	Z_HAS_HOSTNAME 				= 16,
	Z_CAN_LEAVE_MAIL_ON_SERVER 	= 32
} config_options;

class ProtocolConfigView : public BView {
	public:
		ProtocolConfigView(uint32 options_mask = Z_HAS_FLAVORS | Z_HAS_USERNAME | Z_HAS_PASSWORD | Z_HAS_HOSTNAME);
		void SetTo(BMessage *archive);
		
		void AddFlavor(const char *label);
		void AddAuthMethod(const char *label);
		
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);
		virtual	void AttachedToWindow();
		virtual void MessageReceived(BMessage *msg);
};