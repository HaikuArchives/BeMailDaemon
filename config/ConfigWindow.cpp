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
#include <TextView.h>
#include <MenuItem.h>
#include <Screen.h>
#include <PopUpMenu.h>
#include <MenuBar.h>
#include <TabView.h>
#include <Box.h>
#include <Alert.h>
#include <Bitmap.h>
#include <Roster.h>
#include <Resources.h>
#include <Region.h>

#include <Entry.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Path.h>
#include <AppFileInfo.h>

#include <MailSettings.h>

#include <stdio.h>
#include <string.h>


// define if you want to have an apply button
//#define HAVE_APPLY_BUTTON

const char *kEMail = "bemaildaemon-talk@bug-br.org.br";
const char *kMailto = "mailto:bemaildaemon-talk@bug-br.org.br";
const char *kBugsitePretty = "Bug-Tracker at SourceForge.net";
const char *kBugsite = "http://sourceforge.net/tracker/?func=add&group_id=26926&atid=388726";
const char *kWebsite = "http://www.bug-br.org.br/zoidberg/";
const rgb_color kLinkColor = {40,40,180};


const uint32 kMsgAccountSelected = 'acsl';
const uint32 kMsgAddAccount = 'adac';
const uint32 kMsgRemoveAccount = 'rmac';

const uint32 kMsgIntervalUnitChanged = 'iuch';

const uint32 kMsgShowStatusWindowChanged = 'shst';
const uint32 kMsgStatusLookChanged = 'lkch';
const uint32 kMsgStatusWorkspaceChanged = 'wsch';

const uint32 kMsgApplySettings = 'apst';
const uint32 kMsgSaveSettings = 'svst';
const uint32 kMsgRevertSettings = 'rvst';
const uint32 kMsgCancelSettings = 'cnst';

class AccountsListView : public BListView {
	public:
		AccountsListView(BRect rect) : BListView(rect,NULL,B_SINGLE_SELECTION_LIST,B_FOLLOW_ALL) {}
		virtual	void KeyDown(const char *bytes, int32 numBytes) {
			if (numBytes != 1)
				return;
			
			if ((*bytes == B_DELETE) || (*bytes == B_BACKSPACE))
				Window()->PostMessage(kMsgRemoveAccount);
		}
};

class BitmapView : public BView
{
	public:
		BitmapView(BBitmap *bitmap) : BView(bitmap->Bounds(),NULL,B_FOLLOW_NONE,B_WILL_DRAW)
		{
			fBitmap = bitmap;

			SetDrawingMode(B_OP_ALPHA);
		}

		~BitmapView()
		{
			delete fBitmap;
		}

		virtual void AttachedToWindow()
		{
			SetViewColor(Parent()->ViewColor());

			MoveTo((Parent()->Bounds().Width() - Bounds().Width()) / 2,Frame().top);
		}

		virtual void Draw(BRect updateRect)
		{
			DrawBitmap(fBitmap,updateRect,updateRect);
		}

	private:
		BBitmap *fBitmap;
};


class AboutTextView : public BTextView
{
	public:
		AboutTextView(BRect rect) : BTextView(rect,NULL,rect.OffsetToCopy(B_ORIGIN),B_FOLLOW_NONE,B_WILL_DRAW)
		{
			int32 major = 0,middle = 0,minor = 0,variety = 0,internal = 1;

			// get version information for app

			app_info appInfo;
			if (be_app->GetAppInfo(&appInfo) == B_OK)
			{
				BFile file(&appInfo.ref,B_READ_ONLY);
				if (file.InitCheck() == B_OK)
				{
					BAppFileInfo info(&file);
					if (info.InitCheck() == B_OK)
					{
						version_info versionInfo;
						if (info.GetVersionInfo(&versionInfo,B_APP_VERSION_KIND) == B_OK)
						{
							major = versionInfo.major;
							middle = versionInfo.middle;
							minor = versionInfo.minor;
							variety = versionInfo.variety;
							internal = versionInfo.internal;
						}
					}
				}
			}
			// prepare version variety string
			const char *varietyStrings[] = {"Development","Alpha","Beta","Gamma","Golden master","Final"};
			char varietyString[32];
			strcpy(varietyString,varietyStrings[variety % 6]);
			if (variety < 5)
				sprintf(varietyString + strlen(varietyString),"/%li",internal);

			char s[512];
			sprintf(s,	"Mail Daemon Replacement\n\n"
						"by Dr. Zoidberg Enterprises. All rights reserved.\n\n"
						"Version %ld.%ld.%ld %s\n\n"
						"See LICENSE file included in the installation package for more information.\n\n\n\n"
						"You can contact us at:\n"
						"%s\n\n"
						"Please submit bug reports using the %s\n\n"
						"Project homepage at:\n%s",
						major,middle,minor,varietyString,
						kEMail,kBugsitePretty,kWebsite);

			SetText(s);
			MakeEditable(false);
			MakeSelectable(false);

			SetAlignment(B_ALIGN_CENTER);
			SetStylable(true);

			// aethetical changes
			BFont font;
			GetFont(&font);
			font.SetSize(24);
			SetFontAndColor(0,23,&font,B_FONT_SIZE);

			// center the view vertically
			rect = TextRect();  rect.OffsetTo(0,(Bounds().Height() - TextHeight(0,42)) / 2);
			SetTextRect(rect);

			// set the link regions
			int start = strstr(s,kEMail) - s;
			int finish = start + strlen(kEMail);
			GetTextRegion(start,finish,&fMail);
			SetFontAndColor(start,finish,NULL,0,&kLinkColor);

			start = strstr(s,kBugsitePretty) - s;
			finish = start + strlen(kBugsitePretty);
			GetTextRegion(start,finish,&fBugsite);
			SetFontAndColor(start,finish,NULL,0,&kLinkColor);

			start = strstr(s,kWebsite) - s;
			finish = start + strlen(kWebsite);
			GetTextRegion(start,finish,&fWebsite);
			SetFontAndColor(start,finish,NULL,0,&kLinkColor);
		}

		virtual void Draw(BRect updateRect)
		{
			BTextView::Draw(updateRect);
			
			BRect rect(fMail.Frame());
			StrokeLine(BPoint(rect.left,rect.bottom-2),BPoint(rect.right,rect.bottom-2));
			rect = fBugsite.Frame();
			StrokeLine(BPoint(rect.left,rect.bottom-2),BPoint(rect.right,rect.bottom-2));
			rect = fWebsite.Frame();
			StrokeLine(BPoint(rect.left,rect.bottom-2),BPoint(rect.right,rect.bottom-2));
		}

		virtual void MouseDown(BPoint point)
		{
			if (fMail.Contains(point))
			{
				char *arg[] = {(char *)kMailto,NULL};
				be_roster->Launch("text/x-email",1,arg);
			}
			else if (fBugsite.Contains(point))
			{
				char *arg[] = {(char *)kBugsite,NULL};
				be_roster->Launch("text/html",1,arg);
			}
			else if (fWebsite.Contains(point))
			{
				char *arg[] = {(char *)kWebsite,NULL};
				be_roster->Launch("text/html",1,arg);
			}
		}
	
	private:
		BRegion	fWebsite,fMail,fBugsite;
};


//--------------------------------------------------------------------------------------
//	#pragma mark -


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
	fAccountsListView = new AccountsListView(rect);
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

	MakeHowToView();

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

	rect = box->Frame();  rect.bottom = rect.top + 4*height + 20;
	box = new BBox(rect);
	box->SetLabel("Status Window");
	view->AddChild(box);

	BPopUpMenu *statusPopUp = new BPopUpMenu(B_EMPTY_STRING);
	const char *statusModes[] = {"Never","While Sending / Fetching","Always"};
	BMessage *msg;
	for (int32 i = 0;i < 3;i++)
	{
		statusPopUp->AddItem(item = new BMenuItem(statusModes[i],msg = new BMessage(kMsgShowStatusWindowChanged)));
		msg->AddInt32("ShowStatusWindow",i);
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
	const char *windowLookStrings[] = {"Normal, With Tab","Normal, Border Only","Floating","Thin Border","No Border"};
	for (int32 i = 0;i < 5;i++)
	{
		lookPopUp->AddItem(item = new BMenuItem(windowLookStrings[i],msg = new BMessage(kMsgStatusLookChanged)));
		msg->AddInt32("StatusWindowLook",i);
		if (i == 0)
			item->SetMarked(true);
	}
	rect.OffsetBy(0,height + 6);
	fStatusLookField = new BMenuField(rect,"status look","Window Look:",lookPopUp);
	fStatusLookField->SetDivider(labelWidth);
	box->AddChild(fStatusLookField);

	BPopUpMenu *workspacesPopUp = new BPopUpMenu(B_EMPTY_STRING);
	workspacesPopUp->AddItem(item = new BMenuItem("Current Workspace",msg = new BMessage(kMsgStatusWorkspaceChanged)));
	msg->AddInt32("StatusWindowWorkSpace",0);
	workspacesPopUp->AddItem(item = new BMenuItem("All Workspaces",msg = new BMessage(kMsgStatusWorkspaceChanged)));
	msg->AddInt32("StatusWindowWorkSpace",-1);

	rect.OffsetBy(0,height + 6);
	fStatusWorkspaceField = new BMenuField(rect,"status workspace","Window visible on:",workspacesPopUp);
	fStatusWorkspaceField->SetDivider(labelWidth);
	box->AddChild(fStatusWorkspaceField);

	rect = box->Frame();  rect.bottom = rect.top + 2*height + 13;
	box = new BBox(rect);
	box->SetLabel("Deskbar Icon");
	view->AddChild(box);

	rect = box->Bounds().InsetByCopy(8,8);
	rect.left += 100;  rect.right -= 100;
	rect.top += 7;
	BButton *button = new BButton(rect,B_EMPTY_STRING,"Configure Menu Links",msg = new BMessage(B_REFS_RECEIVED));
	box->AddChild(button);
	button->SetTarget(BMessenger("application/x-vnd.Be-TRAK"));

	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY,&path);
	path.Append("Mail/Menu Links");
	BEntry entry(path.Path());
	if (entry.InitCheck() == B_OK && entry.Exists())
	{
		entry_ref ref;
		entry.GetRef(&ref);
		msg->AddRef("refs",&ref);
	}
	else
		button->SetEnabled(false);

	rect = box->Frame();  rect.bottom = rect.top + 2*height + 6;
	box = new BBox(rect);
	box->SetLabel("Misc.");
	view->AddChild(box);

	rect = box->Bounds().InsetByCopy(8,8);
	rect.top += 7;	rect.bottom = rect.top + height + 5;
	fAutoStartCheckBox = new BCheckBox(rect,"start daemon","Auto-Start Mail Daemon",NULL);
	box->AddChild(fAutoStartCheckBox);

	// about page

	rect = tabView->Bounds();	rect.bottom -= tabView->TabHeight() + 4;
	tabView->AddTab(view = new BView(rect,NULL,B_FOLLOW_ALL,0));
	tabView->TabAt(2)->SetLabel("About");
	view->SetViewColor(top->ViewColor());

	AboutTextView *about = new AboutTextView(rect);
	about->SetViewColor(top->ViewColor());
	view->AddChild(about);

	// save/cancel/revert buttons

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
#ifdef HAVE_APPLY_BUTTON
	cancelButton->MoveTo(saveButton->Frame().left - w - 5,rect.top);
#else
	cancelButton->MoveTo(saveButton->Frame().left - w - 20,rect.top);
#endif
	top->AddChild(cancelButton);

#ifdef HAVE_APPLY_BUTTON
	BButton *applyButton = new BButton(rect,"apply","Apply",new BMessage(kMsgApplySettings));
	applyButton->GetPreferredSize(&w,&h);
	applyButton->ResizeTo(w,h);
	applyButton->MoveTo(cancelButton->Frame().left - w - 20,rect.top);
	top->AddChild(applyButton);
#endif

	BButton *revertButton = new BButton(rect,"revert","Revert",new BMessage(kMsgRevertSettings));
	revertButton->GetPreferredSize(&w,&h);
	revertButton->ResizeTo(w,h);
#ifdef HAVE_APPLY_BUTTON
	revertButton->MoveTo(applyButton->Frame().left - w - 5,rect.top);
#else
	revertButton->MoveTo(cancelButton->Frame().left - w - 6,rect.top);
#endif
	top->AddChild(revertButton);

	LoadSettings();

	fAccountsListView->SetSelectionMessage(new BMessage(kMsgAccountSelected));
}


ConfigWindow::~ConfigWindow()
{
}


void ConfigWindow::MakeHowToView()
{
	BResources *resources = BApplication::AppResources();
	if (resources)
	{
		size_t length;
		char *buffer = (char *)resources->FindResource('ICON',101,&length);
		if (buffer)
		{
			BBitmap *bitmap = new BBitmap(BRect(0,0,63,63),B_CMAP8);
			if (bitmap && bitmap->InitCheck() == B_OK)
			{
				// copy and enlarge a 32x32 8-bit bitmap
				char *bits = (char *)bitmap->Bits();
				for (int32 i = 0,j = -64;i < length;i++)
				{
					if ((i % 32) == 0)
						j += 64;

					char *b = bits + (i << 1) + j;
					b[0] = b[1] = b[64] = b[65] = buffer[i];
				}
				fConfigView->AddChild(new BitmapView(bitmap));
			}
			else
				delete bitmap;
		}
	}

	BRect rect = fConfigView->Bounds();
	BTextView *text = new BTextView(rect,NULL,rect,B_FOLLOW_NONE,B_WILL_DRAW);
	text->SetViewColor(fConfigView->Parent()->ViewColor());
	text->SetAlignment(B_ALIGN_CENTER);
	text->SetText("\n\nCreate a new account using the \"Add\" button.\n\nDelete accounts (or only the inbound/outbound) by using the \"Remove\" button on the selected item.\n\nSelect an item in the list to edit its configuration.");
	rect = text->Bounds();
	text->ResizeTo(rect.Width(),text->TextHeight(0,42));
	text->SetTextRect(rect);
	fConfigView->AddChild(text);
	
	static_cast<CenterContainer *>(fConfigView)->Layout();
}


void ConfigWindow::LoadSettings()
{
	Accounts::Delete();
	Accounts::Create(fAccountsListView,fConfigView);

	// load in general settings
	Mail::Settings *settings = new Mail::Settings();
	status_t status = SetToGeneralSettings(settings);
	if (status == B_OK)
	{
		// adjust own window frame
		BScreen screen(this);
		BRect screenFrame(screen.Frame().InsetByCopy(0,5));
		BRect frame(settings->ConfigWindowFrame());

		if (screenFrame.Contains(frame.LeftTop()))
			MoveTo(frame.LeftTop());
		else // center on screen
			MoveTo((screenFrame.Width() - frame.Width()) / 2,(screenFrame.Height() - frame.Height()) / 2);
	}
	else
		fprintf(stderr,"Error retrieving general settings: %s\n", strerror(status));

	delete settings;
}


void ConfigWindow::SaveSettings()
{
	// remove config views
	((CenterContainer *)fConfigView)->DeleteChildren();

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
	Mail::Settings settings;
	if (fSaveSettings)
	{
		settings.SetAutoCheckInterval(time * 1e6);
		settings.SetCheckOnlyIfPPPUp(fPPPActiveCheckBox->Value() == B_CONTROL_ON);

		settings.SetDaemonAutoStarts(fAutoStartCheckBox->Value() == B_CONTROL_ON);
		int32 index = fStatusModeField->Menu()->IndexOf(fStatusModeField->Menu()->FindMarked());
		settings.SetShowStatusWindow(index);
		index = fStatusLookField->Menu()->IndexOf(fStatusLookField->Menu()->FindMarked());
		settings.SetStatusWindowLook(index);
	}
	else
	{
		// restore status window look
		settings.SetStatusWindowLook(settings.StatusWindowLook());
	}

	settings.SetConfigWindowFrame(Frame());
	settings.Save();

	/*** save accounts ***/

	if (fSaveSettings)
		Accounts::Save();

	// start the mail_daemon if auto start was selected
	if (fSaveSettings && fAutoStartCheckBox->Value() == B_CONTROL_ON
		&& !be_roster->IsRunning("application/x-vnd.Be-POST"))
	{
		be_roster->Launch("application/x-vnd.Be-POST");
	}
}


bool ConfigWindow::QuitRequested()
{
	SaveSettings();

	Accounts::Delete();

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
			{
				// deselect current item
				((CenterContainer *)fConfigView)->DeleteChildren();
				MakeHowToView();
				break;
			}
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
				{
					item->account->Remove(item->type);
					MakeHowToView();
				}
			}
			break;
		}

		case kMsgIntervalUnitChanged:
		{
			int32 index;
			if (msg->FindInt32("index",&index) == B_OK)
				fIntervalControl->SetEnabled(index != 0);
			break;
		}

		case kMsgShowStatusWindowChanged:
		case kMsgStatusLookChanged:
		case kMsgStatusWorkspaceChanged:
		{
			// the status window stuff is the only "live" setting
			BMessenger messenger("application/x-vnd.Be-POST");
			if (messenger.IsValid())
				messenger.SendMessage(msg);
			break;
		}

		case kMsgRevertSettings:
			RevertToLastSettings();
			break;
		case kMsgApplySettings:
			fSaveSettings = true;
			SaveSettings();
			MakeHowToView();
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


status_t ConfigWindow::SetToGeneralSettings(Mail::Settings *settings)
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

	fAutoStartCheckBox->SetValue(settings->DaemonAutoStarts());

	if (BMenuItem *item = fStatusModeField->Menu()->ItemAt(settings->ShowStatusWindow()))
		item->SetMarked(true);
	if (BMenuItem *item = fStatusLookField->Menu()->ItemAt(settings->StatusWindowLook()))
		item->SetMarked(true);
	if (BMenuItem *item = fStatusWorkspaceField->Menu()->ItemAt(settings->StatusWindowWorkspaces() != B_ALL_WORKSPACES ? 0 : 1))
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


void ConfigWindow::RevertToLastSettings()
{
	// revert general settings
	Mail::Settings settings;

	// restore status window look
	settings.SetStatusWindowLook(settings.StatusWindowLook());

	status_t status = SetToGeneralSettings(&settings);
	if (status != B_OK)
	{
		char text[256];
		sprintf(text,"\nThe general settings couldn't be reverted.\n\nError retrieving general settings:\n%s\n",strerror(status));
		(new BAlert("Error",text,"Ok",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go();
	}

	// revert account data

	if (fAccountsListView->CurrentSelection() != -1)
		((CenterContainer *)fConfigView)->DeleteChildren();

	Accounts::Delete();
	Accounts::Create(fAccountsListView,fConfigView);

	if (fConfigView->CountChildren() == 0)
		MakeHowToView();
}

