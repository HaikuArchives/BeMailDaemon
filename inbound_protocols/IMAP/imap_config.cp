#include <TextControl.h>

#include <ProtocolConfigView.h>
#include <MailAddon.h>

class IMAPConfig : public ProtocolConfigView {
	public:
		IMAPConfig(BMessage *archive);
		virtual ~IMAPConfig();
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual void GetPreferredSize(float *width, float *height);
};

IMAPConfig::IMAPConfig(BMessage *archive) : ProtocolConfigView(Z_HAS_USERNAME | Z_HAS_PASSWORD | Z_HAS_HOSTNAME | Z_CAN_LEAVE_MAIL_ON_SERVER) {
	SetTo(archive);
		
	((BControl *)(FindView("leave_mail_remote")))->SetValue(B_CONTROL_ON);
	((BControl *)(FindView("leave_mail_remote")))->Hide();
	((BControl *)(FindView("delete_remote_when_local")))->SetEnabled(true);
	((BControl *)(FindView("delete_remote_when_local")))->MoveBy(0,-25);
	
		
	BRect frame = ChildAt(CountChildren() - 1)->Frame();
	frame.top += 25;
	frame.bottom += 25;
	
	BTextControl *folder = new BTextControl(frame,"folder","Folder: ","INBOX",NULL);
	folder->SetDivider(be_plain_font->StringWidth("Folder: "));
	
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
