#ifndef ZOIDBERG_PROTOCOL_CONFIG_VIEW_H
#define ZOIDBERG_PROTOCOL_CONFIG_VIEW_H
/* ProtocolConfigView - the standard config view for all protocols
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <View.h>


namespace Zoidberg {
namespace Mail {

typedef enum {
	MP_HAS_AUTH_METHODS 		= 1,
	MP_HAS_FLAVORS 				= 2,
	MP_HAS_USERNAME 			= 4,
	MP_HAS_PASSWORD 			= 8,
	MP_HAS_HOSTNAME 			= 16,
	MP_CAN_LEAVE_MAIL_ON_SERVER = 32
} config_options;

class ProtocolConfigView : public BView {
	public:
		ProtocolConfigView(uint32 options_mask = MP_HAS_FLAVORS | MP_HAS_USERNAME | MP_HAS_PASSWORD | MP_HAS_HOSTNAME);
		virtual ~ProtocolConfigView();
		
		void SetTo(BMessage *archive);
		
		void AddFlavor(const char *label);
		void AddAuthMethod(const char *label,bool needUserPassword = true);

		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);
		virtual	void AttachedToWindow();
		virtual void MessageReceived(BMessage *msg);
	
	private:
		uint32 _reserved[5];
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_PROTOCOL_CONFIG_VIEW_H */
