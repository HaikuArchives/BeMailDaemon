#include <MenuField.h>
#include <PopUpMenu.h>
#include <Message.h>
#include <TextControl.h>
#include <MenuItem.h>

#include <MailAddon.h>

class RuleFilterConfig : public BView {
	public:
		RuleFilterConfig(BMessage *settings);

		virtual	void MessageReceived(BMessage *msg);
		virtual	void AttachedToWindow();
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);
	private:
		BTextControl *attr, *regex, *arg;
		BPopUpMenu *menu;
		int staging;
};

RuleFilterConfig::RuleFilterConfig(BMessage *settings) : BView(BRect(0,0,260,55),"rulefilter_config", B_FOLLOW_LEFT | B_FOLLOW_TOP, 0) {
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	attr = new BTextControl(BRect(5,5,100,20),"attr","If ","header in lower case (e.g. subject)",NULL);
	attr->SetDivider(be_plain_font->StringWidth("If "));
	if (settings->HasString("attribute"))
		attr->SetText(settings->FindString("attribute"));
	AddChild(attr);
	
	regex = new BTextControl(BRect(101,5,255,20),"attr"," is ","value (can be a regular expression, like *spam*)",NULL);
	regex->SetDivider(be_plain_font->StringWidth(" is "));
	if (settings->HasString("regex"))
		regex->SetText(settings->FindString("regex"));
	AddChild(regex);
	
	arg = new BTextControl(BRect(110,25,255,50),"arg",NULL,"this field is based on the Action",NULL);
	if (settings->HasString("argument"))
		arg->SetText(settings->FindString("argument"));
	AddChild(arg);
	
	if (settings->HasInt32("do_what"))
		staging = settings->FindInt32("do_what");
	else
		staging = -1;
}
	

void RuleFilterConfig::AttachedToWindow() {
	menu = new BPopUpMenu("Action");
	BMenuItem *item = new BMenuItem("Move To", new BMessage('argu'));
	item->SetTarget(this);
	menu->AddItem(item);
	item = new BMenuItem("Set Flags To", new BMessage('argu'));
	item->SetTarget(this);
	menu->AddItem(item);
	item = new BMenuItem("Delete Message", new BMessage('argd'));
	item->SetTarget(this);
	menu->AddItem(item);
	BMenuField *field = new BMenuField(BRect(5,25,110,45),"do_what","Then ",menu,true);
	if (staging >= 0) {
		menu->ItemAt(staging)->SetMarked(true);
		MessageReceived(menu->ItemAt(staging)->Message());
	}
	field->ResizeToPreferred();
	field->SetDivider(be_plain_font->StringWidth("Then "));
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
	switch (msg->what) {
		case 'argu':
			arg->SetEnabled(true);
			break;
		case 'argd':
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