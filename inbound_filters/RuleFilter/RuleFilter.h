#include <Message.h>
#include <List.h>
#include <MailAddon.h>

#include "StringMatcher.h"

typedef enum {
	Z_MOVE_TO,
	Z_TRASH,
	Z_FLAG
} z_mail_action_flags;

class RuleFilter : public MailFilter {
	public:
		RuleFilter(BMessage *settings);
		virtual status_t InitCheck(BString* out_message = NULL);
		virtual MDStatus ProcessMailMessage
		(
			BPositionIO** io_message, BEntry* io_entry,
			BMessage* io_headers, BPath* io_folder, BString* io_uid
		);
	private:
		StringMatcher matcher;
		char *attribute, *arg;
		z_mail_action_flags do_what;
};