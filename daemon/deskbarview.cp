#include <Rect.h>
#include <Bitmap.h>
#include <Entry.h>
#include <Path.h>
#include <Roster.h>
#include <Resources.h>
#include <MenuItem.h>
#include <NodeMonitor.h>
#include <Window.h>
#include <VolumeRoster.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <String.h>
#include <Messenger.h>
#include <Deskbar.h>
#include <NodeInfo.h>
#include <kernel/fs_info.h>
#include <kernel/fs_index.h>

#include <stdio.h>
#include <malloc.h>

#include <MailSettings.h>
#include <MailDaemon.h>
#include "QueryMenu.h"

#include "deskbarview.h"

//-----The following #defines get around a bug in get_image_info on ppc---
#if __INTEL__
#define text_part text
#define text_part_size text_size
#else
#define text_part data
#define text_part_size data_size
#endif

extern "C" _EXPORT BView* instantiate_deskbar_item();
status_t our_image(image_info*);

status_t our_image(image_info* image)
{
	int32 cookie = 0;
	status_t ret;
	while ((ret=get_next_image_info(0,&cookie,image)) == B_OK)
	{
		if ((char*)our_image >= (char*)image->text_part &&
			(char*)our_image <= (char*)image->text_part + image->text_part_size)
			break;
	}
	
	return ret;
}

DeskbarView::DeskbarView(BRect frame) 
		: BView(frame, "mail_daemon", B_FOLLOW_NONE, B_WILL_DRAW|B_PULSE_NEEDED)
		, fIcon(NULL)
		, fCurrentIconState(NEW_MAIL)
		, query(NULL)
		, new_messages(0) {
		
	
}

BView* instantiate_deskbar_item(void) {
	return new DeskbarView(BRect(0, 0, 15, 15));
}

DeskbarView::DeskbarView(BMessage *message)
	: BView(message)
	, fIcon(NULL)
	, fCurrentIconState(NEW_MAIL)
	, query(NULL) {
	
	pop_up = new BPopUpMenu("mail",false,false);
	pop_up->SetFont(be_plain_font);
	
	pop_up->AddItem(new BMenuItem("Create New Message",new BMessage(MD_OPEN_NEW)));
	pop_up->AddItem(new BMenuItem("Open Inbox",new BMessage(MD_OPEN_MAILBOX)));
	pop_up->AddItem(new BMenuItem("Open Mail Folder",new BMessage(MD_OPEN_MAIL_FOLDER)));
	pop_up->AddSeparatorItem();
	
	// here should be menus for new and draft messages
	new_messages_item = new BMenuItem("No new messages",new BMessage(MD_OPEN_NEW_MAIL_QUERY));
	new_messages_item->SetEnabled(false);
	pop_up->AddItem(new_messages_item);
	
	// From BeMail:
	QueryMenu *qmenu;
	qmenu = new QueryMenu( "Open Draft", false );
	qmenu->SetTargetForItems( this );
	fs_create_index( dev_for_path( "/boot/home/mail/draft" ), "MAIL:draft", B_INT32_TYPE, 0 );
	qmenu->SetPredicate( "MAIL:draft==1" );
	pop_up->AddItem( qmenu );
	
	pop_up->AddSeparatorItem();
	
	pop_up->AddItem(new BMenuItem("Check Mail Now",new BMessage(MD_CHECK_SEND_NOW)));
	pop_up->AddItem(new BMenuItem("Edit Preferences",new BMessage(MD_OPEN_PREFS)));


	pop_up->AddSeparatorItem();
	pop_up->AddItem(new BMenuItem("Quit",new BMessage(B_QUIT_REQUESTED)));
	
	ChangeIcon(NO_MAIL);
}

DeskbarView::~DeskbarView() {
	delete fIcon;
}

void DeskbarView::AttachedToWindow() {
	if (be_roster->IsRunning("application/x-vnd.Be-POST")) {
		BVolume boot;
		query = new BQuery;
	
		BVolumeRoster().GetBootVolume(&boot);
		query->SetTarget(this);
		query->SetVolume(&boot);
		query->PushAttr("MAIL:status");
		query->PushString("New");
		query->PushOp(B_EQ);
		query->PushAttr("BEOS:TYPE");
		query->PushString("text/x-email");
		query->PushOp(B_EQ);
		query->PushOp(B_AND);
		query->Fetch();
		
		BEntry entry;
		for (new_messages = 0; query->GetNextEntry(&entry) == B_OK; new_messages++);
	
		ChangeIcon((new_messages > 0) ? NEW_MAIL : NO_MAIL);
	} else {
		BDeskbar *deskbar = new BDeskbar();
		deskbar->RemoveItem("mail_daemon");
		delete deskbar;
	}
}

DeskbarView* DeskbarView::Instantiate(BMessage *data) {
	if (!validate_instantiation(data, "DeskbarView"))
		return NULL;
	return new DeskbarView(data);
}

status_t DeskbarView::Archive(BMessage *data,bool deep) const {
	BView::Archive(data, deep);

	data->AddString("add_on", "application/x-vnd.Be-POST");
	return B_NO_ERROR;
}

void DeskbarView::Draw(BRect /*updateRect*/) {	
	rgb_color oldColor = HighColor();
	SetHighColor(Parent()->ViewColor());
	FillRect(BRect(0.0,0.0,15.0,15.0));
	SetHighColor(oldColor);
	SetDrawingMode(B_OP_OVER);
	if(fIcon)
		DrawBitmap(fIcon,BRect(0.0,0.0,15.0,15.0));
	SetDrawingMode(B_OP_COPY);			
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

void OpenNewMailQuery()
{
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("Mail/New E-mail");

	BEntry query(path.Path());
	if(!query.Exists())
	{
		//create the query file if it doesn't exist
		BFile file(&query, B_READ_WRITE | B_CREATE_FILE);
		if(file.InitCheck() != B_OK)
			return;
		file.Unset();

		BNode node(&query);
		if(node.InitCheck() != B_OK)
			return;
			
		// better way to do this???
		BString string("((MAIL:status==\"[nN][eE][wW]\")&&(BEOS:TYPE==\"text/x-email\"))");
		node.WriteAttrString("_trk/qrystr",&string);
		string = "E-mail";
		node.WriteAttrString("_trk/qryinitmime", &string);
		BNodeInfo(&node).SetType("application/x-vnd.Be-query");
		node.Unset();
	}

	// launch the query
	entry_ref ref;
	if (get_ref_for_path(path.Path(),&ref) != B_OK)
		return;

	BMessage open_query(B_REFS_RECEIVED);
	open_query.AddRef("refs",&ref);
	
	BMessenger tracker("application/x-vnd.Be-TRAK");
	tracker.SendMessage(&open_query);
}

void DeskbarView::MessageReceived(BMessage *message) {
	fprintf(stderr,"got message %lx\n",message->what);
	switch(message->what)
	{
	case MD_CHECK_SEND_NOW:
		MailDaemon::CheckMail(true);
		break;
	case MD_OPEN_NEW:
	{
		char *argv[] = { "New Message", "mailto:" };
		be_roster->Launch("text/x-email",2,argv);
		break;
	}
	case MD_OPEN_PREFS:
		be_roster->Launch("application/x-vnd.Be-mprf");
		break;
	case MD_OPEN_MAILBOX:
		OpenFolder("mail/mailbox");
		break;
	case MD_OPEN_MAIL_FOLDER:
		OpenFolder("mail");
		break;
	case MD_OPEN_NEW_MAIL_QUERY:
		OpenNewMailQuery();
		break;
	case B_QUERY_UPDATE:
	{
		int32 what;
		message->FindInt32("opcode",&what);
		switch (what) {
			case B_ENTRY_CREATED:
				new_messages++;
				break;
			case B_ENTRY_REMOVED:
				new_messages--;
				break;
		}	
		ChangeIcon((new_messages > 0) ? NEW_MAIL : NO_MAIL);
		break;
	}
	case MD_SET_DEFAULT_ACCOUNT:
	{
		if (BMenuItem *item = default_menu->FindMarked())
		{
			MailSettings settings;
			settings.SetDefaultOutboundChainID(message->FindInt32("chain_id"));
			settings.Save();
		}
		break;
	}
	case B_QUIT_REQUESTED:
	{
		BMessenger mess("application/x-vnd.Be-POST");
		if (mess.IsValid())
			mess.SendMessage(message);
		break;
	}
	case B_REFS_RECEIVED:
	{
		BMessage argv(B_ARGV_RECEIVED);
		argv.AddInt32("argc", 2);
		argv.AddString("argv", "E-mail");
		
		entry_ref ref;
		BPath path;
		int i=0;
		
		while (message->FindRef("refs",i++,&ref)==B_OK
				&& path.SetTo(&ref) == B_OK)
		{
			fprintf(stderr,"got %s\n", path.Path());
			argv.AddString("argv", path.Path());
		}
		
		if (i>1) be_roster->Launch("text/x-email", &argv);
		break;
	}
	default:
		BView::MessageReceived(message);
	}
}

void DeskbarView::ChangeIcon(int32 icon) {
	if(fCurrentIconState == icon)
		return;
		
	BBitmap *new_icon(NULL);
	char icon_name[10];
	
	switch(icon) {
		case NO_MAIL:
			strcpy(icon_name, "Read");
			break;
		case NEW_MAIL:
			strcpy(icon_name, "New");
			break;
	}
	{
		image_info info;
		if (our_image(&info)==B_OK)
		{
			BFile file(info.name,B_READ_ONLY);
			if(file.InitCheck() != B_OK)
				goto err;
			BResources rsrc(&file);
			size_t len;
			const void *data = rsrc.LoadResource('BBMP',icon_name, &len);
			if(len == 0)
				goto err;
			BMemoryIO stream(data, len);
			stream.Seek(0, SEEK_SET);
			BMessage archive;
				if (archive.Unflatten(&stream) != B_OK)
				goto err;
			new_icon = new BBitmap(&archive);
		}
		else
			fputs("no image!", stderr);
	}
err:
	fCurrentIconState = icon;
	delete fIcon;
	fIcon = new_icon;
	Invalidate();
}

void DeskbarView::Pulse() {
	// Check if mail_daemon is still running
}

void DeskbarView::MouseUp(BPoint pos) {
	if (buttons & B_PRIMARY_MOUSE_BUTTON) OpenFolder("mail/mailbox");
	if (buttons & B_TERTIARY_MOUSE_BUTTON) MessageReceived(new BMessage(MD_CHECK_SEND_NOW));
}

void DeskbarView::MouseDown(BPoint pos) {
	Looper()->CurrentMessage()->FindInt32("buttons",&buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		BString string;
		if (new_messages > 0)
			string << new_messages;
		else
			string << "No";
		
		string << " new message" << ((new_messages != 1) ? "s" : B_EMPTY_STRING);
		new_messages_item->SetLabel(string.String());
		new_messages_item->SetEnabled(new_messages > 0);

		ConvertToScreen(&pos);
		pop_up->SetTargetForItems(this);
		pop_up->Go(pos,true,true,BRect(pos.x - 2, pos.y - 2, pos.x + 2, pos.y + 2),true);
	}
}
