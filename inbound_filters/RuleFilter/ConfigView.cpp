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
#include <MailSettings.h>

using namespace Zoidberg;

const uint32 kMsgActionMoveTo = 'argm';
const uint32 kMsgActionDelete = 'argd';
const uint32 kMsgActionSetTo = 'args';
const uint32 kMsgReplyWith = 'argr';


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
		BPopUpMenu *menu, *outbound;
		BMenuField *outbound_field;
		int staging;
		int32 chain;
};

#include <stdio.h>

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
	
	outbound = new BPopUpMenu("<Choose Account>");
	BList list;
	Mail::OutboundChains(&list);
	if (settings->HasInt32("do_what"))
		staging = settings->FindInt32("do_what");
	else
		staging = -1;
	if (staging == 3)
		chain = settings->FindInt32("argument");
	else
		chain = -1;
	printf("Chain: %d\n",chain);
	for (int32 i = 0; i < list.CountItems(); i++) {
		BMenuItem *item = new BMenuItem(((Mail::Chain *)(list.ItemAt(i)))->Name(), new BMessage(((Mail::Chain *)(list.ItemAt(i)))->ID()));
		outbound->AddItem(item);
		if (((Mail::Chain *)(list.ItemAt(i)))->ID() == chain)
			item->SetMarked(true);
		delete (Mail::Chain *)(list.ItemAt(i));
	}
		
}
	

void RuleFilterConfig::AttachedToWindow() {
	menu = new BPopUpMenu("<Choose Action>");
	menu->AddItem(new BMenuItem("Move To", new BMessage(kMsgActionMoveTo)));
	menu->AddItem(new BMenuItem("Set Flags To", new BMessage(kMsgActionSetTo)));
	menu->AddItem(new BMenuItem("Delete Message", new BMessage(kMsgActionDelete)));
	menu->AddItem(new BMenuItem("Reply With", new BMessage(kMsgReplyWith)));
	menu->SetTargetForItems(this);

	BMenuField *field = new BMenuField(BRect(5,30,210,50),"do_what","Then",menu);
	field->ResizeToPreferred();
	field->SetDivider(be_plain_font->StringWidth("Then") + 8);
	AddChild(field);
	
	outbound_field = new BMenuField(BRect(5,55,255,80),"reply","Foo",outbound);
	outbound_field->ResizeToPreferred();
	outbound_field->SetDivider(0);
	if (staging >= 0) {
		menu->ItemAt(staging)->SetMarked(true);
		MessageReceived(menu->ItemAt(staging)->Message());
	} else {
		AddChild(arg);
	}
}

status_t RuleFilterConfig::Archive(BMessage *into, bool deep) const {
	into->MakeEmpty();
	into->AddInt32("do_what",menu->IndexOf(menu->FindMarked()));
	into->AddString("attribute",attr->Text());
	into->AddString("regex",regex->Text());
	if (into->FindInt32("do_what") == 3) {
		printf("foo!");
		into->AddInt32("argument",outbound->FindMarked()->Message()->what);
	} else
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
			if (arg->Parent() == NULL) {
				outbound_field->RemoveSelf();
				AddChild(arg);
			}
			break;
		case kMsgActionDelete:
			arg->SetEnabled(false);
			if (arg->Parent() == NULL) {
				outbound_field->RemoveSelf();
				AddChild(arg);
			}
			break;
		case kMsgReplyWith:
			if (outbound->Parent() == NULL) {
				arg->RemoveSelf();
				AddChild(outbound_field);
			}
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
