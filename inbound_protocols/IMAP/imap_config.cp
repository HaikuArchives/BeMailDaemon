/* IMAPConfig - config view for the IMAP protocol add-on
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <TextControl.h>

#include <ProtocolConfigView.h>
#include <MailAddon.h>

#include <MDRLanguage.h>

using namespace Zoidberg;


class IMAPConfig : public Mail::ProtocolConfigView {
	public:
		IMAPConfig(BMessage *archive);
		virtual ~IMAPConfig();
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual void GetPreferredSize(float *width, float *height);
};

IMAPConfig::IMAPConfig(BMessage *archive)
	: Mail::ProtocolConfigView(Mail::MP_HAS_USERNAME | Mail::MP_HAS_PASSWORD | Mail::MP_HAS_HOSTNAME | Mail::MP_CAN_LEAVE_MAIL_ON_SERVER)
{
	SetTo(archive);
		
	((BControl *)(FindView("leave_mail_remote")))->SetValue(B_CONTROL_ON);
	((BControl *)(FindView("leave_mail_remote")))->Hide();
	((BControl *)(FindView("delete_remote_when_local")))->SetEnabled(true);
	((BControl *)(FindView("delete_remote_when_local")))->MoveBy(0,-25);
	
		
	BRect frame = ChildAt(CountChildren() - 1)->Frame();
	frame.top += 25;
	frame.bottom += 25;
	
	BTextControl *folder = new BTextControl(frame,"folder",MDR_DIALECT_CHOICE ("Folder: ","受信箱： "),"INBOX",NULL);
	folder->SetDivider(be_plain_font->StringWidth(MDR_DIALECT_CHOICE ("Folder: ","受信箱： ")));
	
	archive->PrintToStream();
	
	if (archive->HasString("folder"))
		folder->SetText(archive->FindString("folder"));
	
	AddChild(folder);
		
	ResizeToPreferred();
}

IMAPConfig::~IMAPConfig() {}

status_t IMAPConfig::Archive(BMessage *into, bool deep) const {
	ProtocolConfigView::Archive(into,deep);
	
	if (into->ReplaceString("folder",((BTextControl *)(FindView("folder")))->Text()) != B_OK)
		into->AddString("folder",((BTextControl *)(FindView("folder")))->Text());
	
	into->PrintToStream();
	
	return B_OK;
}

void IMAPConfig::GetPreferredSize(float *width, float *height) {
	ProtocolConfigView::GetPreferredSize(width,height);
	*height -= 25;
}

BView* instantiate_config_panel(BMessage *settings,BMessage *) {
	return new IMAPConfig(settings);
}
