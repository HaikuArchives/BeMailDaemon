#include <TextControl.h>

#include <ProtocolConfigView.h>
#include <MailAddon.h>

class IMAPConfig : public ProtocolConfigView {
	public:
		IMAPConfig(BMessage *archive);
		virtual ~IMAPConfig();
		virtual	status_t Archive(BMessage *into, bool deep = true) const;
};

IMAPConfig::IMAPConfig(BMessage *archive) : ProtocolConfigView(Z_HAS_USERNAME | Z_HAS_PASSWORD | Z_HAS_HOSTNAME | Z_CAN_LEAVE_MAIL_ON_SERVER) {
	BRect frame = ChildAt(CountChildren() - 1)->Frame();
	frame.top += 25;
	frame.bottom += 25;
	
	BTextControl *folder = new BTextControl(frame,"folder","Folder: ","INBOX",NULL);
	folder->SetDivider(be_plain_font->StringWidth("Folder: "));
	
	archive->PrintToStream();
	
	if (archive->HasString("folder"))
		folder->SetText(archive->FindString("folder"));
		
	AddChild(folder);
	SetTo(archive);
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

BView* instantiate_config_panel(BMessage *settings) {
	return new IMAPConfig(settings);
}