#include <Message.h>
#include <List.h>
#include <MailAddon.h>

#include "StringMatcher.h"

#if B_BEOS_VERSION_DANO 
	#define DANO_ONLY(x) x
#else
	#define DANO_ONLY(x)
#endif

typedef enum {
	Z_MOVE_TO,
	Z_FLAG,
	Z_TRASH
} z_mail_action_flags;

class RuleFilter : public MailFilter {
	public:
							RuleFilter(BMessage *settings);
		virtual				~RuleFilter() { }					
		virtual status_t	InitCheck(BString* out_message = NULL);
		
		virtual MDStatus	ProcessMailMessage(BPositionIO** io_message,
											   BEntry* io_entry,
											   BMessage* io_headers,
											   BPath* io_folder,
											   BString* io_uid);
	private:
		StringMatcher			matcher;
		DANO_ONLY(const) char*	attribute;
		DANO_ONLY(const) char*	arg;
		z_mail_action_flags		do_what;
};

