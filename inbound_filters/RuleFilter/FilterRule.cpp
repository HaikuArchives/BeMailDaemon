#include "RuleFilter.h"
#include "StringMatcher.h"

FilterRule::FilterRule(BMessage *settings) : actions(settings) {
	actions->FindString("attribute",&attribute);
	
	const char *regex;
	actions->FindString("regex",&regex);
	matcher.SetPattern(regex,true);
}
	
	
FilterRule::~FilterRule() {
	delete actions;
}

MDStatus FilterRule::DoStuff(BMessage *headers,BPath *server_path) {
	const char *to_match = NULL;
	headers->FindString(attribute,&to_match);
	
	if (to_match == NULL)
		return MD_OK;
		
	if (!matcher.Match(to_match))
		return MD_OK;
		
	BMessage action;
	for (int32 i = 0; actions->FindMessage("action",i,&action) == B_OK; i++) {
		switch (action.what) {
			case Z_MOVE_TO:
				if (headers->ReplaceString("destination",action.FindString("field")) != B_OK)
					headers->AddString("destination",action.FindString("field"));
					
				break;
			case Z_TRASH:
				return MD_DISCARD;
			case Z_FLAG:
				if (headers->ReplaceString(action.FindString("field1"),action.FindString("field2")) != B_OK)
					headers->AddString(action.FindString("field1"),action.FindString("field2"));
				
				break;
		}
	}
	
	return MD_HANDLED;
}