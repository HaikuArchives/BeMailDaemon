/* RuleFilter's config view - performs action depending on matching a header value
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <MenuField.h>
#include <PopUpMenu.h>
#include <Message.h>
#include <TextControl.h>
#include <MenuItem.h>

#include <MailAddon.h>
#include <FileConfigView.h>

using namespace Zoidberg;

const uint32 kMsgActionMoveTo = 'argm';
const uint32 kMsgActionDelete = 'argd';
const uint32 kMsgActionSetTo = 'args';


class RuleFilterConfig : public BView {
	public:
		RuleFilterConfig(BMessage *settings);

		virtual	void MessageReceived(BMessage *msg);
		virtual	void AttachedToWindow();
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);
	private:
		BTextControl *attr, *regex;
		FileControl *arg;
		BPopUpMenu *menu;
		int staging;
};

RuleFilterConfig::RuleFilterConfig(BMessage *settings) : BView(BRect(0,0,260,85),"rulefilter_config", B_FOLLOW_LEFT | B_FOLLOW_TOP, 0) {
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	attr = new BTextControl(BRect(5,5,100,20),"attr","If","header (e.g. Subject)",NULL);
	attr->SetDivider(be_plain_font->StringWidth("If ") + 4);
	if (settings->HasString("attribute"))
		attr->SetText(settings->FindString("attribute"));
	AddChild(attr);
	
	regex = new BTextControl(BRect(104,5,255,20),"attr"," is ","value (can be a regular expression, like *spam*)",NULL);
	regex->SetDivider(be_plain_font->StringWidth(" is ") + 4);
	if (settings->HasString("regex"))
		regex->SetText(settings->FindString("regex"));
	AddChild(regex);
	
	arg = new FileControl(BRect(5,55,255,80),"arg",NULL,"this field is based on the Action");
	if (BControl *control = (BControl *)arg->FindView("select_file"))
		control->SetEnabled(false);
	if (settings->HasString("argument"))
		arg->SetText(settings->FindString("argument"));
	AddChild(arg);
	
	if (settings->HasInt32("do_what"))
		staging = settings->FindInt32("do_what");
	else
		staging = -1;
}
	

void RuleFilterConfig::AttachedToWindow() {
	menu = new BPopUpMenu("<Choose Action>");
	menu->AddItem(new BMenuItem("Move To", new BMessage(kMsgActionMoveTo)));
	menu->AddItem(new BMenuItem("Set Flags To", new BMessage(kMsgActionSetTo)));
	menu->AddItem(new BMenuItem("Delete Message", new BMessage(kMsgActionDelete)));
	menu->SetTargetForItems(this);

	BMenuField *field = new BMenuField(BRect(5,30,210,50),"do_what","Then",menu);
	if (staging >= 0) {
		menu->ItemAt(staging)->SetMarked(true);
		MessageReceived(menu->ItemAt(staging)->Message());
	}
	field->ResizeToPreferred();
	field->SetDivider(be_plain_font->StringWidth("Then") + 8);
	AddChild(field);
}

status_t RuleFilterConfig::Archive(BMessage *into, bool deep) const {
	into->MakeEmpty();
	into->AddInt32("do_what",menu->IndexOf(menu->FindMarked()));
	into->AddString("attribute",attr->Text());
	into->AddString("regex",regex->Text());
	into->AddString("argument",arg->Text());
	
	return B_OK;
}

void RuleFilterConfig::MessageReceived(BMessage *msg) {
	switch (msg->what)
	{
		case kMsgActionMoveTo:
		case kMsgActionSetTo:
			if (BControl *control = (BControl *)arg->FindView("file_path")) 
				arg->SetEnabled(true);
			if (BControl *control = (BControl *)arg->FindView("select_file"))
				control->SetEnabled(msg->what == kMsgActionMoveTo);
			break;
		case kMsgActionDelete:
			arg->SetEnabled(false);
			break;
		default:
			BView::MessageReceived(msg);
	}
}

void RuleFilterConfig::GetPreferredSize(float *width, float *height) {
	*width = 260;
	*height = 55;
}

BView* instantiate_config_panel(BMessage *settings,BMessage *metadata) {
	return new RuleFilterConfig(settings);
}
