/* ConfigWindow - main eMail config window
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigWindow.h"
#include "CenterContainer.h"
#include "Account.h"

#include <Application.h>
#include <ListView.h>
#include <ScrollView.h>
#include <Button.h>
#include <CheckBox.h>
#include <MenuField.h>
#include <TextControl.h>
#include <MenuItem.h>
#include <Screen.h>
#include <PopUpMenu.h>
#include <MenuBar.h>
#include <TabView.h>
#include <Box.h>

#include <Entry.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Path.h>

#include <MailSettings.h>

#include <stdio.h>
#include <string.h>


const uint32 kMsgAccountSelected = 'acsl';
const uint32 kMsgAddAccount = 'adac';
const uint32 kMsgRemoveAccount = 'rmac';

const uint32 kMsgIntervalUnitChanged = 'iuch';
const uint32 kMsgStatusLookChanged = 'lkch';

const uint32 kMsgSaveSettings = 'svst';
const uint32 kMsgRevertSettings = 'rvst';
const uint32 kMsgCancelSettings = 'cnst';


ConfigWindow::ConfigWindow()
		: BWindow(BRect(200.0, 200.0, 640.0, 580.0),
				  "E-mail", B_TITLED_WINDOW,
				  B_ASYNCHRONOUS_CONTROLS | B_NOT_ZOOMABLE | B_NOT_CLOSABLE | B_NOT_RESIZABLE),
		fLastSelectedAccount(NULL),
		fSaveSettings(false)
{
	/*** create controls ***/

	BRect rect(Bounds());
	BView *top = new BView(rect,NULL,B_FOLLOW_ALL,0);
	top->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(top);

	// determine font height
	font_height fontHeight;
	top->GetFontHeight(&fontHeight);
	int32 height = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 5;

	rect.InsetBy(5,5);	rect.bottom -= 11 + height;
	BTabView *tabView = new BTabView(rect,NULL);

	BView *view,*generalView;
	rect = tabView->Bounds();  rect.bottom -= tabView->TabHeight() + 4;
	tabView->AddTab(view = new BView(rect,NULL,B_FOLLOW_ALL,0));
	tabView->TabAt(0)->SetLabel("Accounts");
	view->SetViewColor(top->ViewColor());

	// accounts listview

	rect = view->Bounds().InsetByCopy(8,8);
	rect.right = 140 - B_V_SCROLL_BAR_WIDTH;
	rect.bottom -= height + 12;
	fAccountsListView = new BListView(rect,NULL,B_SINGLE_SELECTION_LIST,B_FOLLOW_ALL);
	view->AddChild(new BScrollView(NULL,fAccountsListView,B_FOLLOW_ALL,0,false,true));
	rect.right += B_V_SCROLL_BAR_WIDTH;

	rect.top = rect.bottom + 8;  rect.bottom = rect.top + height;
	BRect sizeRect = rect;	sizeRect.right = sizeRect.left + 30 + view->StringWidth("Add");
	view->AddChild(new BButton(sizeRect,NULL,"Add",new BMessage(kMsgAddAccount),B_FOLLOW_BOTTOM));

	sizeRect.left = sizeRect.right+3;	sizeRect.right = sizeRect.left + 30 + view->StringWidth("Remove");
	view->AddChild(fRemoveButton = new BButton(sizeRect,NULL,"Remove",new BMessage(kMsgRemoveAccount),B_FOLLOW_BOTTOM));

	// accounts config view
	rect = view->Bounds();
	rect.left = fAccountsListView->Frame().right + B_V_SCROLL_BAR_WIDTH + 16;
	rect.right -= 10;
	view->AddChild(fConfigView = new CenterContainer(rect));

	// general settings

	rect = tabView->Bounds();	rect.bottom -= tabView->TabHeight() + 4;
	tabView->AddTab(view = new CenterContainer(rect));
	tabView->TabAt(1)->SetLabel("General");

	rect = view->Bounds().InsetByCopy(8,8);
	rect.right -= 1;	rect.bottom = rect.top + height * 3 + 15;
	BBox *box = new BBox(rect);
	box->SetLabel("Retrieval Frequency");
	view->AddChild(box);

	rect = box->Bounds().InsetByCopy(8,8);
	rect.top += 7;	rect.bottom = rect.top + height + 5;
	BRect tile = rect.OffsetByCopy(0,1);
	int32 labelWidth = (int32)view->StringWidth("Check every:")+6;
	tile.right = 80 + labelWidth;
	fIntervalControl = new BTextControl(tile,"time","Check every:",NULL,NULL);
	fIntervalControl->SetDivider(labelWidth);
	box->AddChild(fIntervalControl);

	BPopUpMenu *frequencyPopUp = new BPopUpMenu(B_EMPTY_STRING);
	const char *frequencyStrings[] = {"Never","Minutes","Hours","Days"};
	BMenuItem *item;
	for (int32 i = 0;i < 4;i++)
	{
		frequencyPopUp->AddItem(item = new BMenuItem(frequencyStrings[i],new BMessage(kMsgIntervalUnitChanged)));
		if (i == 1)
			item->SetMarked(true);
	}
	tile.left = tile.right + 5;  tile.right = rect.right;
	tile.OffsetBy(0,-1);
	fIntervalUnitField = new BMenuField(tile,"frequency",B_EMPTY_STRING,frequencyPopUp);
	fIntervalUnitField->SetDivider(0.0);
	box->AddChild(fIntervalUnitField);

	rect.OffsetBy(0,height + 9);	rect.bottom -= 2;
	fPPPActiveCheckBox = new BCheckBox(rect,"ppp active","only when PPP is active", NULL);
	box->AddChild(fPPPActiveCheckBox);

	rect = box->Frame();//  rect.bottom = rect.top + 3*height + 15;
	box = new BBox(rect);
	box->SetLabel("Status Window");
	view->AddChild(box);

	BPopUpMenu *statusPopUp = new BPopUpMenu(B_EMPTY_STRING);
	const char *statusModes[] = {"Never","While Sending / Fetching","When Idle","Always"};
	for (int32 i = 0;i < 4;i++)
	{
		statusPopUp->AddItem(item = new BMenuItem(statusModes[i], NULL));
		if (i == 0)
			item->SetMarked(true);
	}
	rect = box->Bounds().InsetByCopy(8,8);
	rect.top += 7;	rect.bottom = rect.top + height + 5;
	labelWidth = (int32)view->StringWidth("Show Status Window:") + 8;
	fStatusModeField = new BMenuField(rect,"show status","Show Status Window:",statusPopUp);
	fStatusModeField->SetDivider(labelWidth);
	box->AddChild(fStatusModeField);

	BPopUpMenu *lookPopUp = new BPopUpMenu(B_EMPTY_STRING);
	BMessage *msg;
	const char *windowLookStrings[] = {"Normal, With Tab","Normal, Border Only","Floating","Thin Border","No Border"};
	const int32 windowLooks[] = {MD_STATUS_LOOK_TITLED,MD_STATUS_LOOK_NORMAL_BORDER,MD_STATUS_LOOK_FLOATING,MD_STATUS_LOOK_THIN_BORDER,MD_STATUS_LOOK_NO_BORDER};
	for (int32 i = 0;i < 5;i++)
	{
		lookPopUp->AddItem(item = new BMenuItem(windowLookStrings[i],msg = new BMessage(kMsgStatusLookChanged)));
		msg->AddInt32("look",windowLooks[i]);
		if (i == 0)
			item->SetMarked(true);
	}
	rect.OffsetBy(0,height + 6);
	fStatusLookField = new BMenuField(rect,"status look","Window Look:",lookPopUp);
	fStatusLookField->SetDivider(labelWidth);
	box->AddChild(fStatusLookField);

	rect = box->Frame();  rect.bottom = rect.top + 2*height + 6;
	box = new BBox(rect);
	box->SetLabel("Misc.");
	view->AddChild(box);

	rect = box->Bounds().InsetByCopy(8,8);
	rect.top += 7;	rect.bottom = rect.top + height + 5;
	fAutoStartCheckBox = new BCheckBox(rect,"start daemon","Auto-Start Mail Daemon",NULL);
	box->AddChild(fAutoStartCheckBox);

	top->AddChild(tabView);

	rect = tabView->Frame();
	rect.top = rect.bottom + 5;  rect.bottom = rect.top + height + 5;
	BButton *saveButton = new BButton(rect,"save","Save",new BMessage(kMsgSaveSettings));
	float w,h;
	saveButton->GetPreferredSize(&w,&h);
	saveButton->ResizeTo(w,h);
	saveButton->MoveTo(rect.right - w,rect.top);
	top->AddChild(saveButton);

	BButton *cancelButton = new BButton(rect,"cancel","Cancel",new BMessage(kMsgCancelSettings));
	cancelButton->GetPreferredSize(&w,&h);
	cancelButton->ResizeTo(w,h);
	cancelButton->MoveTo(saveButton->Frame().left - w - 20,rect.top);
	top->AddChild(cancelButton);

	BButton *revertButton = new BButton(rect,"revert","Revert",new BMessage(kMsgRevertSettings));
	revertButton->GetPreferredSize(&w,&h);
	revertButton->ResizeTo(w,h);
	revertButton->MoveTo(cancelButton->Frame().left - w - 6,rect.top);
	top->AddChild(revertButton);

	LoadSettings();

	fAccountsListView->SetSelectionMessage(new BMessage(kMsgAccountSelected));
//	fAccountsListView->Select(0);
//	fAccountsListView->MakeFocus(true);
}


ConfigWindow::~ConfigWindow()
{
}


void ConfigWindow::LoadSettings()
{
	Accounts::Delete();
	Accounts::Create(fAccountsListView,fConfigView);

	// load in accounts
//	Account *account;
//	bool first = true;
//	BList accountList = Account::List();
//	for (int32 i = 0; (account = (Account*)accountList.ItemAt(i)); i++)
//	{
//		fAccountsListView->AddItem(account);
//		if (first)
//		{
//			SetToAccount(account);
//			first = false;
//		}
//	}

	// load in general settings
	MailSettings *settings = new MailSettings();
	status_t status = SetToGeneralSettings(settings);
	if (status == B_OK)
	{
		// adjust own window frame
		BRect frame = settings->ConfigWindowFrame();
		BScreen screen(this);
		if (screen.Frame().Contains(frame.LeftTop()))
			MoveTo(frame.LeftTop());
	}
	else
		printf("Error retrieving general settings: %s\n", strerror(status));

	delete settings;
}


void ConfigWindow::SaveSettings()
{
	/*** save general settings ***/

	// figure out time interval
	float interval;
	sscanf(fIntervalControl->Text(),"%f",&interval);
	float multiplier = 0;
	switch (fIntervalUnitField->Menu()->IndexOf(fIntervalUnitField->Menu()->FindMarked()))
	{
		case 1:		// minutes
			multiplier = 60;
			break;
		case 2:		// hours
			multiplier = 60 * 60;
			break;
		case 3:		// days
			multiplier = 24 * 60 * 60;
			break;
	}
	time_t time = (time_t)(multiplier * interval);

	// apply and save general settings
	MailSettings settings;
	if (fSaveSettings)
	{
		settings.SetAutoCheckInterval(time * 1e6);
		settings.SetCheckOnlyIfPPPUp(fPPPActiveCheckBox->Value() == B_CONTROL_ON);

//		mail_notification how;
//		how.alert = mAlertOnNewMailCB->Value() == B_CONTROL_ON;
//		how.beep = mBeepOnNewMailCB->Value() == B_CONTROL_ON;
//		settings.SetNotifyType(how);

		settings.SetDaemonAutoStarts(fAutoStartCheckBox->Value() == B_CONTROL_ON);
		int32 index = fStatusModeField->Menu()->IndexOf(fStatusModeField->Menu()->FindMarked());
		settings.SetShowStatusWindow(index);
		index = fStatusLookField->Menu()->IndexOf(fStatusLookField->Menu()->FindMarked());
		settings.SetStatusWindowLook(index);
	}
	settings.SetConfigWindowFrame(Frame());
	settings.Save();

	/*** save accounts ***/

	if (fSaveSettings)
		Accounts::Save();
}


bool ConfigWindow::QuitRequested()
{
	// remove config views
	for (int32 i = fConfigView->CountChildren();i-- > 0;)
	{
		BView *view = fConfigView->ChildAt(i);
		if (fConfigView->RemoveChild(view))
			delete view;
	}

	SaveSettings();

	Accounts::Delete();

/*
	// This here will leave us with some kind of zombie mail_daemon!

	if (fSaveSettings && fAutoStartCheckBox->Value() == B_CONTROL_ON
		&& !be_roster->IsRunning("application/x-vnd.Be-POST"))
	{
		be_roster->Launch("application/x-vnd.Be-POST");
	}
*/
	
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void ConfigWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case kMsgAccountSelected:
		{
			int32 index;
			if (msg->FindInt32("index",&index) != B_OK || index < 0)
				break;
			AccountItem *item = (AccountItem *)fAccountsListView->ItemAt(index);
			if (item)
				item->account->Selected(item->type);
			break;
		}
		case kMsgAddAccount:
		{
			Accounts::NewAccount();
			break;
		}
		case kMsgRemoveAccount:
		{
			int32 index = fAccountsListView->CurrentSelection();
			if (index >= 0)
			{
				AccountItem *item = (AccountItem *)fAccountsListView->ItemAt(index);
				if (item)
					item->account->Remove(item->type);
			}
			break;
		}

#ifdef NIX
		case kMsgAccountSelected:
		{
			// apply settings of previously selected account
			if (mLastSelectedAccount)
				ApplySettings(mLastSelectedAccount->settings);

			// apply settings of new account
			mLastSelectedAccount = (AccountItem *)mAccountsLV->ItemAt(mAccountsLV->CurrentSelection());
			if (mLastSelectedAccount)
				SetToAccount(mLastSelectedAccount->settings);
			else
				DisableAccountControls();
			break;
		}
		case MSG_ADD_ACCOUNT:
		{
			// create new account and apply its default settings
			if (mLastSelectedAccount)
				ApplySettings(mLastSelectedAccount->settings);
			mLastSelectedAccount = new AccountItem(new MailAccount(), true);
			mAccountsLV->AddItem(mLastSelectedAccount);
			mAccountsLV->Select(mAccountsLV->IndexOf(mLastSelectedAccount));
			SetToAccount(mLastSelectedAccount->settings);
			mAccountNameTC->MakeFocus(true);
			PostMessage(B_SELECT_ALL, mAccountNameTC->TextView());
			break;
		}
		case MSG_REMOVE_ACCOUNT:
		{
			if (mLastSelectedAccount
				&& mAccountsLV->RemoveItem(mLastSelectedAccount))
			{
				if (!mLastSelectedAccount->is_new)
				{
					mDeletedAccounts.AddItem((void *)mLastSelectedAccount->settings);
				}
				else
				{
					mLastSelectedAccount->settings->Delete();
					delete mLastSelectedAccount->settings;
					delete mLastSelectedAccount;
				}
				mLastSelectedAccount = NULL;
			}
			break;
		}
		case MSG_RESTORE_ACCOUNT:
		{
			// restore previous content of the account
			if (mLastSelectedAccount)
				SetToAccount(mLastSelectedAccount->settings);
			break;
		}
		case MSG_ADDON_CHANGED:
		{
			// rebuild menu
			BPopUpMenu *menu;
			if (msg->FindPointer("auths",(void **)&menu) == B_OK)
			{
				mAuthTypePU->MenuBar()->RemoveItem(0L);
				mAuthTypePU->MenuBar()->AddItem(menu);
			}
			if (msg->FindPointer("out_auths",(void **)&menu) == B_OK)
			{
				mOutAuthTypePU->MenuBar()->RemoveItem(0L);
				mOutAuthTypePU->MenuBar()->AddItem(menu);
			}
			break;
		}
		case MSG_ACCOUNT_NAME_CHANGED:
		{
			if (mLastSelectedAccount)
				ApplySettings(mLastSelectedAccount->settings);
			break;
		}
		case MSG_LEAVE_MSG_CHANGED:
		{
			if (mLeaveOnServerCB->Value() == B_CONTROL_ON)
			{
				mDeleteOnServerCB->SetEnabled(true);
			}
			else
			{
				mDeleteOnServerCB->SetValue(B_CONTROL_OFF);
				mDeleteOnServerCB->SetEnabled(false);
			}
			break;
		}
		case MSG_FETCH_MAIL_CHANGED:
		{
			// disable all incoming mail controls?
			break;
		}
		case MSG_CONFIGURE_FILTERS:
		{
			if (mLastSelectedAccount)
				(new FilterConfigWindow(this, mLastFilterWindowPos, mLastSelectedAccount->settings))->Show();
			break;
		}
#endif
		case kMsgIntervalUnitChanged:
		{
			int32 index;
			if (msg->FindInt32("index",&index) == B_OK)
				fIntervalControl->SetEnabled(index != 0);
			break;
		}
		case kMsgStatusLookChanged:
		{
			BMessenger messenger("application/x-vnd.Be-POST");
			if (messenger.IsValid())
				messenger.SendMessage(msg);
			break;
		}

		case kMsgRevertSettings:
			RevertToLastSettings();
			break;
		case kMsgSaveSettings:
			fSaveSettings = true;
			PostMessage(B_QUIT_REQUESTED);
			break;
		case kMsgCancelSettings:
			fSaveSettings = false;
			PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BWindow::MessageReceived(msg);
			break;
	}
}

#ifdef NIX
void ConfigWindow::DisableAccountControls()
{
	mAccountNameTC->SetText("");
	mRealNameTC->SetText("");
	mReturnAddressTC->SetText("");
	mMailHostTC->SetText("");
	mUserNameTC->SetText("");
	mPasswordTC->SetText("");
	mOutUserNameTC->SetText("");
	mOutPasswordTC->SetText("");
	mOutgoingMailHostTC->SetText("");

	mAccountNameTC->SetEnabled(false);
	mRealNameTC->SetEnabled(false);
	mReturnAddressTC->SetEnabled(false);
	mMailHostTC->SetEnabled(false);
	mUserNameTC->SetEnabled(false);
	mPasswordTC->SetEnabled(false);
	mOutgoingMailHostTC->SetEnabled(false);
	mOutUserNameTC->SetEnabled(false);
	mOutPasswordTC->SetEnabled(false);

	mServerTypePU->SetEnabled(false);
	mAuthTypePU->SetEnabled(false);
	mOutServerTypePU->SetEnabled(false);
	mOutAuthTypePU->SetEnabled(false);

	mLeaveOnServerCB->SetValue(B_CONTROL_OFF);
	mDeleteOnServerCB->SetValue(B_CONTROL_OFF);

	mLeaveOnServerCB->SetEnabled(false);
	mDeleteOnServerCB->SetEnabled(false);
	mFetchMailCB->SetEnabled(false);
	
//	mRestoreAccountB->SetEnabled(false);
	mRemoveAccountB->SetEnabled(false);
//	mConfigureFiltersB->SetEnabled(false);
}

// SetToAccount
void ConfigWindow::SetToAccount(MailAccount *selected)
{
	if (selected)
	{
		// read in settings of the account
		mAccountNameTC->SetText(selected->AccountName());
		mRealNameTC->SetText(selected->RealName());
		mReturnAddressTC->SetText(selected->ReturnAddress());
		mMailHostTC->SetText(selected->MailHost());
		mUserNameTC->SetText(selected->UserName());
		mPasswordTC->SetText(selected->Password());

		mAccountNameTC->SetEnabled(true);
		mRealNameTC->SetEnabled(true);
		mReturnAddressTC->SetEnabled(true);
		mMailHostTC->SetEnabled(true);
		mUserNameTC->SetEnabled(true);
		mPasswordTC->SetEnabled(true);
		mOutUserNameTC->SetEnabled(true);
		mOutPasswordTC->SetEnabled(true);
		mOutgoingMailHostTC->SetEnabled(true);

		mServerTypePU->SetEnabled(true);
		if (selected->MailProtocolAddOn())
		{
			if (BMenuItem *item = mServerTypePU->Menu()->FindItem(selected->MailProtocolAddOn()))
				item->SetMarked(true);
		}

		mLeaveOnServerCB->SetEnabled(true);
		mLeaveOnServerCB->SetValue(selected->LeaveMailOnServer() ? B_CONTROL_ON : B_CONTROL_OFF);
		mDeleteOnServerCB->SetEnabled(true);
		mDeleteOnServerCB->SetValue(selected->DeleteRemoteWhenLocal() ? B_CONTROL_ON : B_CONTROL_OFF);
		mFetchMailCB->SetEnabled(true);
		mFetchMailCB->SetValue(selected->FetchMail() ? B_CONTROL_ON : B_CONTROL_OFF);
		if (mLeaveOnServerCB->Value() == B_CONTROL_ON)
		{
			mDeleteOnServerCB->SetEnabled(true);
		}
		else
		{
			mDeleteOnServerCB->SetValue(B_CONTROL_OFF);
			mDeleteOnServerCB->SetEnabled(false);
		}

		mAuthTypePU->SetEnabled(true);
		if (BMenuItem *item = mServerTypePU->Menu()->FindMarked())
		{
			BMessage *msg = item->Message();
			BPopUpMenu *menu;
			if (msg->FindPointer("auths",(void **)&menu) == B_OK)
			{
				if ((item = menu->ItemAt(selected->AuthIndex())))
					item->SetMarked(true);
				menu->SetFont(be_plain_font);
				mAuthTypePU->MenuBar()->RemoveItem(0L);
				mAuthTypePU->MenuBar()->AddItem(menu);
			}
		}

		mOutServerTypePU->SetEnabled(true);
		if (selected->OutgoingMailProtocolAddOn())
		{
			if (BMenuItem *item = mOutServerTypePU->Menu()->FindItem(selected->OutgoingMailProtocolAddOn()))
				item->SetMarked(true);
		}
		
		mOutAuthTypePU->SetEnabled(true);
		if (BMenuItem *item = mOutServerTypePU->Menu()->FindMarked())
		{
			BMessage *msg = item->Message();
			BPopUpMenu *menu;
			if (msg->FindPointer("out_auths",(void **)&menu) == B_OK)
			{
				if ((item = menu->ItemAt(selected->OutboundAuthIndex())))
					item->SetMarked(true);
				menu->SetFont(be_plain_font);
				mOutAuthTypePU->MenuBar()->RemoveItem(0L);
				mOutAuthTypePU->MenuBar()->AddItem(menu);
			}
		}

		mOutgoingMailHostTC->SetText(selected->OutgoingMailHost());
		mOutUserNameTC->SetText(selected->OutboundUserName());
		mOutPasswordTC->SetText(selected->OutboundPassword());

//		mRestoreAccountB->SetEnabled(true);
		mRemoveAccountB->SetEnabled(true);
//		mConfigureFiltersB->SetEnabled(true);
	}
}
#endif

status_t ConfigWindow::SetToGeneralSettings(MailSettings *settings)
{
	if (!settings)
		return B_BAD_VALUE;

	status_t status = settings->InitCheck();
	if (status != B_OK)
		return status;

	// retrieval frequency

	time_t interval = time_t(settings->AutoCheckInterval() / 1e6L);
	char text[25];
	text[0] = 0;
	int timeIndex = 0;
	if (interval >= 60)
	{
		timeIndex = 1;
		sprintf(text,"%ld",interval / (60));
	}
	if (interval >= (60*60))
	{
		timeIndex = 2;
		sprintf(text,"%ld",interval / (60*60));
	}
	if (interval >= (60*60*24))
	{
		timeIndex = 3;
		sprintf(text,"%ld",interval / (60*60*24));
	}
	fIntervalControl->SetText(text);

	if (BMenuItem *item = fIntervalUnitField->Menu()->ItemAt(timeIndex))
		item->SetMarked(true);
	fIntervalControl->SetEnabled(timeIndex != 0);

	fPPPActiveCheckBox->SetValue(settings->CheckOnlyIfPPPUp());

//	mBeepOnNewMailCB->SetValue(how.beep);
//	mAlertOnNewMailCB->SetValue(how.alert);

	fAutoStartCheckBox->SetValue(settings->DaemonAutoStarts());

//	if (BMenuItem *item = mTimeTypePU->Menu()->ItemAt(time_type_index))
//		item->SetMarked(true);
//	else if (BMenuItem *item = mTimeTypePU->Menu()->ItemAt(0))
//		item->SetMarked(true);

	if (BMenuItem *item = fStatusModeField->Menu()->ItemAt(settings->ShowStatusWindow()))
		item->SetMarked(true);
	if (BMenuItem *item = fStatusLookField->Menu()->ItemAt(settings->StatusWindowLook()))
		item->SetMarked(true);

	BMessenger messenger("application/x-vnd.Be-POST");
	if (messenger.IsValid())
	{
		BMessage msg(kMsgStatusLookChanged);
		msg.AddInt32("look", settings->StatusWindowLook());
		messenger.SendMessage(&msg);
	}

	return B_OK;
}

#ifdef NIX
void ConfigWindow::ApplySettings(MailAccount *account)
{
	if (account)
	{
		// apply values of controls to acount
		account->SetAccountName(mAccountNameTC->Text());
		if (mLastSelectedAccount)
		{
			mLastSelectedAccount->SetText(mAccountNameTC->Text());
			mAccountsLV->InvalidateItem(mAccountsLV->IndexOf(mLastSelectedAccount));
		}
		account->SetRealName(mRealNameTC->Text());
		account->SetReturnAddress(mReturnAddressTC->Text());
		account->SetMailHost(mMailHostTC->Text());
		account->SetUserName(mUserNameTC->Text());
		account->SetPassword(mPasswordTC->Text());
		account->SetOutboundUserName(mOutUserNameTC->Text());
		account->SetOutboundPassword(mOutPasswordTC->Text());

		if (BMenuItem *item = mServerTypePU->Menu()->FindMarked())
			account->SetMailProtocol(item->Label());

		if (BMenuItem *item = mAuthTypePU->MenuBar()->SubmenuAt(0)->FindMarked())
			account->SetAuth(mAuthTypePU->MenuBar()->SubmenuAt(0)->IndexOf(item));

		account->SetLeaveMailOnServer(mLeaveOnServerCB->Value() == B_CONTROL_ON);
		account->SetDeleteRemoteWhenLocal(mDeleteOnServerCB->Value() == B_CONTROL_ON);
		account->SetOutgoingMailHost(mOutgoingMailHostTC->Text());
		account->SetFetchMail(mFetchMailCB->Value() == B_CONTROL_ON);

		if (BMenuItem *marked_item = mOutServerTypePU->Menu()->FindMarked())
			account->SetOutgoingMailProtocol(marked_item->Label());

		if (BMenuItem *item = mOutAuthTypePU->MenuBar()->SubmenuAt(0)->FindMarked())
			account->SetOutboundAuth(mOutAuthTypePU->MenuBar()->SubmenuAt(0)->IndexOf(item));
	}
}
#endif


void ConfigWindow::RevertToLastSettings()
{
	// revert general settings
	MailSettings *settings = new MailSettings();
	status_t status = SetToGeneralSettings(settings);
	if (status != B_OK)
		printf("Error retrieving general settings: %s\n", strerror(status));
	delete settings;

	// revert account data
//	for (int32 i = 0; AccountItem *item = (AccountItem *)mAccountsLV->ItemAt(i); i++)
//	{
//		if (item->is_new)
//		{
//			item->settings->Delete();
//			delete item->settings;
//			mAccountsLV->RemoveItem(item);
//			delete item;
//			i--;
//		}
//		else
//		{
//			item->settings->Reload();
//		}
//	}
//	while (MailAccount *account = (MailAccount *)mDeletedAccounts.RemoveItem(0L))
//	{
//		mAccountsLV->AddItem(new AccountItem(account, false));
//	}
//	if ((mLastSelectedAccount = (AccountItem *)mAccountsLV->ItemAt(0)))
//	{
//		mAccountsLV->Select(0);
//		SetToAccount(mLastSelectedAccount->settings);
//	}
//	else
//		DisableAccountControls();
}

