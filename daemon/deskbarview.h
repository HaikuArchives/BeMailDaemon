#include <View.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <Query.h>

enum MDDeskbarIcon {
	NO_MAIL = 0,
	NEW_MAIL,
};

enum MDDeskbarMessages {
	MD_CHECK_SEND_NOW = 'MDra',
	MD_SET_DEFAULT_ACCOUNT,
	MD_OPEN_NEW,
	MD_OPEN_MAILBOX,
	MD_OPEN_MAIL_FOLDER,
	MD_OPEN_PREFS,
	MD_OPEN_NEW_MAIL_QUERY
};

class _EXPORT DeskbarView : public BView {
public:
				DeskbarView (BRect frame);
				DeskbarView (BMessage *data);	

				~DeskbarView();
	
	void		Draw(BRect updateRect);
	virtual	void AttachedToWindow();
	static 		DeskbarView *Instantiate(BMessage *data);
				status_t Archive(BMessage *data, bool deep = true) const;
	void	 	MouseDown(BPoint);
	void	 	MouseUp(BPoint);
	void		MessageReceived(BMessage *message);
	void		ChangeIcon(int32 icon);
	void		Pulse();

private:
	BBitmap 		*fIcon;
	int32			fCurrentIconState;
	BPopUpMenu		*pop_up;
	BMenu			*default_menu;
	BMenuItem		*new_messages_item;
	
	BQuery *query;
	int32 new_messages;
	int32 buttons;
};
