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
		BList filters;
};

class FilterRule {
	public:
		FilterRule(BMessage *settings);
		~FilterRule();
		
		virtual MDStatus DoStuff(BMessage *headers,BPath *server_path);
	private:
		StringMatcher matcher;
		BMessage *actions;
		const char *attribute;
};