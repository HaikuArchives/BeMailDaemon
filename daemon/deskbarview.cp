/* DeskbarView - main_daemon's deskbar menu and view
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/

#include <PopUpMenu.h>
#include <Bitmap.h>
#include <Rect.h>
#include <MenuItem.h>
#include <Window.h>
#include <Messenger.h>
#include <Roster.h>
#include <Resources.h>
#include <Entry.h>
#include <Path.h>
#include <NodeMonitor.h>
#include <VolumeRoster.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Query.h>
#include <SymLink.h>
#include <NodeInfo.h>
#include <kernel/fs_info.h>
#include <kernel/fs_index.h>
#include <Deskbar.h>
#include <String.h>

#include <stdio.h>
#include <malloc.h>

#include <MailSettings.h>
#include <MailDaemon.h>

#include "deskbarview.h"

const char *kTrackerSignature = "application/x-vnd.Be-TRAK";


//-----The following #defines get around a bug in get_image_info on ppc---
#if __INTEL__
#define text_part text
#define text_part_size text_size
#else
#define text_part data
#define text_part_size data_size
#endif

extern "C" _EXPORT BView* instantiate_deskbar_item();


status_t our_image(image_info* image)
{
	int32 cookie = 0;
	status_t ret;
	while ((ret = get_next_image_info(0,&cookie,image)) == B_OK)
	{
		if ((char*)our_image >= (char*)image->text_part &&
			(char*)our_image <= (char*)image->text_part + image->text_part_size)
			break;
	}
	
	return ret;
}

BView* instantiate_deskbar_item(void)
{
	return new DeskbarView(BRect(0, 0, 15, 15));
}


//-------------------------------------------------------------------------------
//	#pragma mark -


DeskbarView::DeskbarView(BRect frame) 
	: BView(frame, "mail_daemon", B_FOLLOW_NONE, B_WILL_DRAW | B_PULSE_NEEDED)
	, fIcon(NULL)
	, fCurrentIconState(NEW_MAIL)
	, fNewMailQuery(NULL)
{
}

DeskbarView::DeskbarView(BMessage *message)
	: BView(message)
	, fIcon(NULL)
	, fCurrentIconState(NEW_MAIL)
	, fNewMailQuery(NULL)
{
	ChangeIcon(NO_MAIL);
}

DeskbarView::~DeskbarView()
{
	delete fIcon;
}

void DeskbarView::AttachedToWindow()
{
	if (be_roster->IsRunning("application/x-vnd.Be-POST"))
	{
		BVolume boot;
		fNewMailQuery = new BQuery;
	
		BVolumeRoster().GetBootVolume(&boot);
		fNewMailQuery->SetTarget(this);
		fNewMailQuery->SetVolume(&boot);
		fNewMailQuery->PushAttr("MAIL:status");
		fNewMailQuery->PushString("New");
		fNewMailQuery->PushOp(B_EQ);
		fNewMailQuery->PushAttr("BEOS:TYPE");
		fNewMailQuery->PushString("text/x-email");
		fNewMailQuery->PushOp(B_EQ);
		fNewMailQuery->PushOp(B_AND);
		fNewMailQuery->Fetch();
		
		BEntry entry;
		for (fNewMessages = 0; fNewMailQuery->GetNextEntry(&entry) == B_OK;)
			if (entry.InitCheck() == B_OK)
				fNewMessages++;

		ChangeIcon((fNewMessages > 0) ? NEW_MAIL : NO_MAIL);
	}
	else
	{
		BDeskbar deskbar;
		deskbar.RemoveItem("mail_daemon");
	}
}

DeskbarView* DeskbarView::Instantiate(BMessage *data)
{
	if (!validate_instantiation(data, "DeskbarView"))
		return NULL;

	return new DeskbarView(data);
}

status_t DeskbarView::Archive(BMessage *data,bool deep) const
{
	BView::Archive(data, deep);

	data->AddString("add_on", "application/x-vnd.Be-POST");
	return B_NO_ERROR;
}

void DeskbarView::Draw(BRect /*updateRect*/)
{
	PushState();

	SetHighColor(Parent()->ViewColor());
	FillRect(BRect(0.0,0.0,15.0,15.0));
	if(fIcon)
	{
		SetDrawingMode(B_OP_OVER);
		DrawBitmap(fIcon,BRect(0.0,0.0,15.0,15.0));
	}

	PopState();
	Sync();
}

void OpenFolder(const char* end)
{
	BPath path;
	find_directory(B_USER_DIRECTORY, &path);
	path.Append(end);
	
	entry_ref ref;
	if (get_ref_for_path(path.Path(),&ref) != B_OK) return;

	BMessage open_mbox(B_REFS_RECEIVED);
	open_mbox.AddRef("refs",&ref);
	
	BMessenger tracker("application/x-vnd.Be-TRAK");
	tracker.SendMessage(&open_mbox);
}

void DeskbarView::MessageReceived(BMessage *message)
{
	switch(message->what)
	{
		case MD_CHECK_SEND_NOW:
			// also happens in DeskbarView::MouseUp() with
			// B_TERTIARY_MOUSE_BUTTON pressed
			MailDaemon::CheckMail(true);
			break;
		case MD_CHECK_FOR_MAILS:
			MailDaemon::CheckMail(false);
			break;
		case MD_SEND_MAILS:
			MailDaemon::SendQueuedMail();
			break;

		case MD_OPEN_NEW:
		{
			char *argv[] = {"New Message", "mailto:"};
			be_roster->Launch("text/x-email",2,argv);
			break;
		}
		case MD_OPEN_PREFS:
			be_roster->Launch("application/x-vnd.Be-mprf");
			break;

		case B_QUERY_UPDATE:
		{
			int32 what;
			message->FindInt32("opcode",&what);
			switch (what) {
				case B_ENTRY_CREATED:
					fNewMessages++;
					break;
				case B_ENTRY_REMOVED:
					fNewMessages--;
					break;
			}	
			ChangeIcon((fNewMessages > 0) ? NEW_MAIL : NO_MAIL);
			break;
		}
		case B_QUIT_REQUESTED:
		{
			BMessenger mess("application/x-vnd.Be-POST");
			if (mess.IsValid())
				mess.SendMessage(message);
			break;
		}
		case B_REFS_RECEIVED:	// open received files in the standard mail application
		{
			BMessage argv(B_ARGV_RECEIVED);
			argv.AddString("argv", "E-mail");
			
			entry_ref ref;
			BPath path;
			int i = 0;
			
			while (message->FindRef("refs",i++,&ref) == B_OK && path.SetTo(&ref) == B_OK)
			{
				//fprintf(stderr,"got %s\n", path.Path());
				argv.AddString("argv", path.Path());
			}
			
			if (i > 1)
			{
				argv.AddInt32("argc", i);
				be_roster->Launch("text/x-email", &argv);
			}
			break;
		}
		default:
			BView::MessageReceived(message);
	}
}

void DeskbarView::ChangeIcon(int32 icon)
{
	if(fCurrentIconState == icon)
		return;
		
	BBitmap *newIcon(NULL);

	image_info info;
	if (our_image(&info) == B_OK)
	{
		BFile file(info.name,B_READ_ONLY);
		if (file.InitCheck() < B_OK)
			goto err;

		BResources rsrc(&file);
		size_t len;
		const void *data = rsrc.LoadResource('BBMP',icon == NEW_MAIL ? "New" : "Read",&len);
		if (len == 0)
			goto err;

		BMemoryIO stream(data, len);
		stream.Seek(0, SEEK_SET);
		BMessage archive;
		if (archive.Unflatten(&stream) != B_OK)
			goto err;
		newIcon = new BBitmap(&archive);
	}
	else
		fputs("no image!", stderr);

err:
	fCurrentIconState = icon;
	delete fIcon;
	fIcon = newIcon;
	Invalidate();
}

void DeskbarView::Pulse()
{
	// Check if mail_daemon is still running
}

void DeskbarView::MouseUp(BPoint pos)
{
	if (fLastButtons & B_PRIMARY_MOUSE_BUTTON)
		OpenFolder("mail/mailbox");

	if (fLastButtons & B_TERTIARY_MOUSE_BUTTON)
		MailDaemon::CheckMail(true);
}

void DeskbarView::MouseDown(BPoint pos)
{
	Looper()->CurrentMessage()->FindInt32("buttons",&fLastButtons);

	if (fLastButtons & B_SECONDARY_MOUSE_BUTTON)
	{
		ConvertToScreen(&pos);
		
		BPopUpMenu *menu = BuildMenu();
		if (menu)
			menu->Go(pos,true,true,BRect(pos.x - 2, pos.y - 2, pos.x + 2, pos.y + 2),true);
	}
}

bool DeskbarView::CreateMenuLinks(BDirectory &directory,BPath &path)
{
	status_t status = directory.SetTo(path.Path());
	if (status == B_OK)
		return true;

	// check if the directory has to be created (and do it in this case)

	BEntry entry(path.Path());
	if (status != B_ENTRY_NOT_FOUND
		|| entry.GetParent(&directory) < B_OK
		|| directory.CreateDirectory(path.Leaf(), NULL) < B_OK
		|| directory.SetTo(path.Path()) < B_OK)
		return false;
	
	BPath targetPath;
	find_directory(B_USER_DIRECTORY, &targetPath);
	targetPath.Append("mail/in");
	
	directory.CreateSymLink("Open Inbox Folder",targetPath.Path(),NULL);
	targetPath.GetParent(&targetPath);
	directory.CreateSymLink("Open Mail Folder",targetPath.Path(),NULL);

	// create the draft query
	
	BFile file;
	if (directory.CreateFile("Open Draft",&file) < B_OK)
		return true;

	BString string("MAIL:draft==1");
	file.WriteAttrString("_trk/qrystr",&string);
	string = "E-mail";
	file.WriteAttrString("_trk/qryinitmime", &string);
	BNodeInfo(&file).SetType("application/x-vnd.Be-query");

	return true;
}

void DeskbarView::CreateNewMailQuery(BEntry &query)
{
	BFile file(&query, B_READ_WRITE | B_CREATE_FILE);
	if(file.InitCheck() != B_OK)
		return;

	BString string("((MAIL:status==\"[nN][eE][wW]\")&&(BEOS:TYPE==\"text/x-email\"))");
	file.WriteAttrString("_trk/qrystr",&string);
	string = "E-mail";
	file.WriteAttrString("_trk/qryinitmime", &string);
	BNodeInfo(&file).SetType("application/x-vnd.Be-query");
}

BPopUpMenu *DeskbarView::BuildMenu()
{
	BPopUpMenu *menu = new BPopUpMenu(B_EMPTY_STRING,false,false);
	menu->SetFont(be_plain_font);
	
	menu->AddItem(new BMenuItem("Create New Message" B_UTF8_ELLIPSIS,new BMessage(MD_OPEN_NEW)));

	menu->AddSeparatorItem();

	BMessenger tracker(kTrackerSignature);
	BNavMenu *navMenu;
	BMenuItem *item;
	BMessage *msg;
	entry_ref ref;

	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("Mail/Menu Links");
	
	BDirectory directory;
	if (CreateMenuLinks(directory,path))
	{
		int32 count = 0;

		while (directory.GetNextRef(&ref) == B_OK)
		{
			count++;

			path.SetTo(&ref);
			//the true here dereferences the symlinks all the way :)
			BEntry entry(&ref, true);

			// do we want to use the NavMenu, or just an ordinary BMenuItem?
			// we are using the NavMenu only for directories and queries
			bool useNavMenu = false;

			if (entry.InitCheck() == B_OK)
			{
				if (entry.IsDirectory())
					useNavMenu = true;
				else if (entry.IsFile())
				{
					// Files should use the BMenuItem unless they are queries
					char mimeString[B_MIME_TYPE_LENGTH];
					BNode node(&entry);
					BNodeInfo info(&node);
					if (info.GetType(mimeString) == B_OK
						&& strcmp(mimeString, "application/x-vnd.Be-query") == 0)
						useNavMenu = true;
				}
				//clobber the existing ref only if the symlink derefernces completely, 
				//otherwise we'll stick with what we have
				entry.GetRef(&ref);
			}

			msg = new BMessage(B_REFS_RECEIVED);
			msg->AddRef("refs", &ref);
			
			if (useNavMenu)
			{
				item = new BMenuItem(navMenu = new BNavMenu(path.Leaf(),B_REFS_RECEIVED,tracker), msg);
				navMenu->SetNavDir(&ref);
			}
			else
				item = new BMenuItem(path.Leaf(), msg);

			menu->AddItem(item);
			if(entry.InitCheck() != B_OK)
				item->SetEnabled(false);
		}
		if (count > 0)
			menu->AddSeparatorItem();
	}

	// The New E-mail query

	if (fNewMessages > 0)
	{
		BString string;
		string << fNewMessages << " new message" << ((fNewMessages != 1) ? "s" : B_EMPTY_STRING);

		find_directory(B_USER_SETTINGS_DIRECTORY, &path);
		path.Append("Mail/New E-mail");

		BEntry query(path.Path());
		if (!query.Exists())
			CreateNewMailQuery(query);
		query.GetRef(&ref);

		item = new BMenuItem(
			navMenu = new BNavMenu(string.String(),B_REFS_RECEIVED,BMessenger(kTrackerSignature)),
			msg = new BMessage(B_REFS_RECEIVED));
		msg->AddRef("refs", &ref);
		navMenu->SetNavDir(&ref);

		menu->AddItem(item);
	}
	else
	{
		menu->AddItem(item = new BMenuItem("No new messages",NULL));
		item->SetEnabled(false);
	}
	
	if (modifiers() & B_SHIFT_KEY)
	{
		menu->AddItem(new BMenuItem("Check For Mails Only",new BMessage(MD_CHECK_FOR_MAILS)));
		menu->AddItem(new BMenuItem("Send Pending Mails",new BMessage(MD_SEND_MAILS)));
	}
	else
		menu->AddItem(new BMenuItem("Check For Mail Now",new BMessage(MD_CHECK_SEND_NOW)));
	
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Edit Preferences" B_UTF8_ELLIPSIS,new BMessage(MD_OPEN_PREFS)));
	menu->AddItem(new BMenuItem("Quit",new BMessage(B_QUIT_REQUESTED)));
	
	// Reset Item Targets

	for (int32 i = menu->CountItems();i-- > 0;)
	{
		item = menu->ItemAt(i);
		if (item && (msg = item->Message()) != NULL)
		{
			if (msg->what == B_REFS_RECEIVED)
				item->SetTarget(tracker);
			else
				item->SetTarget(this);
		}
	}
	return menu;
}

