#ifndef CONFIG_WINDOW_H
#define CONFIG_WINDOW_H
/* ConfigWindow - main eMail config window
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Window.h>
#include <List.h>

class BPopup;
class BTextControl;
class BCheckBox;
class BListView;
class BButton;
class BMenuField;

class MailSettings;
class Account;


class ConfigWindow : public BWindow
{
	public:
		ConfigWindow();
		~ConfigWindow();

		virtual bool	QuitRequested();
		virtual void	MessageReceived(BMessage* msg);

	private:
		void			LoadSettings();
		void			SaveSettings();

		status_t		SetToGeneralSettings(MailSettings *general);
		void			RevertToLastSettings();

//		void		DisableAccountControls();
//		void		SetToAccount(MailAccount *selected);
//		void		ApplySettings(MailAccount *to_account);

	private:
		BListView		*fAccountsListView;
		Account			*fLastSelectedAccount;
		BView			*fConfigView;
		BButton			*fRemoveButton;

		BTextControl	*fIntervalControl;
		BMenuField		*fIntervalUnitField;
		BCheckBox		*fPPPActiveCheckBox;
		BMenuField		*fStatusModeField,*fStatusLookField;
		BCheckBox		*fAutoStartCheckBox;

		bool			fSaveSettings;
};

#endif	/* CONFIG_WINDOW_H */
