#include "RuleFilter.h"

class StatusView;

RuleFilter::RuleFilter(BMessage *settings) : MailFilter(settings) {
	BMessage *message = new BMessage;
	for (int32 i = 0; settings->FindMessage("filter_rule",i,message) == B_OK; i++) {
		filters.AddItem(new FilterRule(message));
		message = new BMessage;
	}
	delete message;
}

status_t RuleFilter::InitCheck(BString* out_message) {
	return B_OK;
}

MDStatus RuleFilter::ProcessMailMessage
(
	BPositionIO** , BEntry* ,
	BMessage* io_headers, BPath* io_folder, BString* 
) {
	for (int32 i = 0; i < filters.CountItems(); i++) {
		MDStatus result = ((FilterRule *)(filters.ItemAt(i)))->DoStuff(io_headers,io_folder);
		switch (result) {
			case MD_HANDLED:
				break;
			case MD_DISCARD:
				return MD_DISCARD;
		}
	}
	return MD_OK;
}

MailFilter* instantiate_mailfilter(BMessage* settings,StatusView *)
{ return new RuleFilter(settings); }
