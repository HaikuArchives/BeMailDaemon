/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2001, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

BeMail(TM), Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/

//--------------------------------------------------------------------
//	
//	Header.cpp
//
//--------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <StorageKit.h>
#include <InterfaceKit.h>
#include <StringView.h>
#include <E-mail.h>
#include <String.h>
#include <fs_index.h>
#include <fs_info.h>
#include <map>

#include <MailSettings.h>
#include <MailMessage.h>

#include "Mail.h"
#include "Header.h"
#include "Utilities.h"
#include "QueryMenu.h"
#include "FieldMsg.h"


using namespace Zoidberg;

extern uint32 gDefaultChain;

const char	*kDateLabel = "Date:";
const uint32 kMsgFrom = 'hFrm';


class QPopupMenu : public QueryMenu
{
	public:
		QPopupMenu(const char *title);

	private:		
		void AddPersonItem(const entry_ref *ref, ino_t node, BString &name, BString &email,
							const char *attr, BMenu *groupMenu, BMenuItem *superItem);

	protected:
		virtual void EntryCreated(const entry_ref &ref, ino_t node);
		virtual void EntryRemoved(ino_t node);

		int32 fGroups;
};

//====================================================================

struct evil
{
	bool operator()(const BString *s1, const BString *s2) const
	{
		return (s1->Compare(*s2) < 0);
	}
};

//====================================================================


THeaderView::THeaderView(BRect rect,BRect windowRect,bool incoming,Mail::Message *mail,bool resending)
	:	BBox(rect, "m_header", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW, B_NO_BORDER),
		fAccountMenu(NULL),
		fChain(gDefaultChain),
		fAccountTo(NULL),
		fAccount(NULL),
		fBcc(NULL),
		fCc(NULL),
		fSubject(NULL),
		fTo(NULL),
		fDate(NULL),
		fIncoming(incoming),
		fResending(resending),
		fBccMenu(NULL),
		fCcMenu(NULL),
		fToMenu(NULL)
{
	BFont font = *be_plain_font;
	font.SetSize(FONT_SIZE);
	SetFont(&font);
	float x = font.StringWidth("Enclosures: ") + 9;
	float y = TO_FIELD_V;

	if (!fIncoming)
	{
		InitEmailCompletion();
		InitGroupCompletion();
	}

	BRect r;
	char string[20];
	if (fIncoming && !resending)
	{
		r.Set(x - font.StringWidth(FROM_TEXT) - 11, y,
			  windowRect.Width() - SEPARATOR_MARGIN, y + TO_FIELD_HEIGHT);
		sprintf(string, FROM_TEXT);
	}
	else
	{
		r.Set(x - 11, y, windowRect.Width() - SEPARATOR_MARGIN, y + TO_FIELD_HEIGHT);
		string[0] = 0;
	}
	y += FIELD_HEIGHT;
	fTo = new TTextControl(r, string, new BMessage(TO_FIELD), fIncoming, resending,
						   B_FOLLOW_LEFT_RIGHT);

	if (!fIncoming)
	{
		fTo->SetChoiceList(&fEmailList);
		fTo->SetAutoComplete(true);
	}
	AddChild(fTo);
	BMessage *msg = new BMessage(FIELD_CHANGED);
	msg->AddInt32("bitmask", FIELD_TO);
	fTo->SetModificationMessage(msg);

	BMenuField	*field;
	if (!fIncoming || resending)
	{
		r.right = r.left + 8;
		r.left = r.right - be_plain_font->StringWidth(TO_TEXT) - 30;
		r.top -= 1;
		fToMenu = new QPopupMenu(TO_TEXT);
		field = new BMenuField(r, "", "", fToMenu, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
		field->SetDivider(0.0);
		field->SetEnabled(true);
		AddChild(field);
	}

	//	"From:" accounts Menu
	if (!fIncoming || resending)
	{
		fAccountMenu = new BPopUpMenu(B_EMPTY_STRING);
		//fAccountMenu->SetRadioMode(true);

		BList chains;
		if (Mail::OutboundChains(&chains) >= B_OK)
		{
			BMessage *msg;
			bool marked = false;
			for (int32 i = 0;i < chains.CountItems();i++)
			{
				Mail::Chain *chain = (Mail::Chain *)chains.ItemAt(i);
				BString name = chain->Name();
				if ((msg = chain->MetaData()) != NULL)
				{
					name << ":   " << msg->FindString("real_name")
						 << "  <" << msg->FindString("reply_to") << ">";
				}
				BMenuItem *item = new BMenuItem(name.String(),msg = new BMessage(kMsgFrom));

				msg->AddInt32("id",chain->ID());

				if (gDefaultChain == chain->ID())
				{
					item->SetMarked(true);
					marked = true;
				}
				fAccountMenu->AddItem(item);
				delete chain;
			}
			if (!marked)
			{
				BMenuItem *item = fAccountMenu->ItemAt(0);
				if (item != NULL)
				{
					item->SetMarked(true);
					fChain = item->Message()->FindInt32("id");
				}
				else
				{
					fAccountMenu->AddItem(item = new BMenuItem("<none>",NULL));
					item->SetEnabled(false);
					fChain = ~0UL;
				}
				// default chain is invalid, set to marked
				gDefaultChain = fChain;
			}
		}
		r.Set(x - font.StringWidth(FROM_TEXT) - 11, y - 1,
			  windowRect.Width() - SEPARATOR_MARGIN, y + TO_FIELD_HEIGHT);
		y += FIELD_HEIGHT;
		field = new BMenuField(r, "account", "From:", fAccountMenu,B_FOLLOW_TOP,
								B_WILL_DRAW | B_NAVIGABLE | B_NAVIGABLE_JUMP);
		field->SetFont(&font);
		field->SetDivider(font.StringWidth("From:") + 11);
		//menu->SetAlignment(B_ALIGN_RIGHT);
		AddChild(field);
	}
	else	// To: account
	{
		bool account = count_pop_accounts() > 0;

		r.Set(x - font.StringWidth(TO_TEXT) - 11, y,
			  windowRect.Width() - SEPARATOR_MARGIN, y + TO_FIELD_HEIGHT);
		if (account)
			r.right -= SEPARATOR_MARGIN + ACCOUNT_FIELD_WIDTH;
		fAccountTo = new TTextControl(r, TO_TEXT, NULL, fIncoming, false, B_FOLLOW_LEFT_RIGHT);
		fAccountTo->SetEnabled(false);
		AddChild(fAccountTo);

		if (account)
		{
			r.left = r.right + 6;  r.right = windowRect.Width() - SEPARATOR_MARGIN;
			fAccount = new TTextControl(r, ACCOUNT_TEXT, NULL, fIncoming, false, B_FOLLOW_RIGHT | B_FOLLOW_TOP);
			fAccount->SetEnabled(false);
			AddChild(fAccount);
		}
		y += FIELD_HEIGHT;
	}

	--y;
	r.Set(x - font.StringWidth(SUBJECT_TEXT) - 11, y,
		  windowRect.Width() - SEPARATOR_MARGIN, y + TO_FIELD_HEIGHT);
	y += FIELD_HEIGHT;
	fSubject = new TTextControl(r, SUBJECT_TEXT, new BMessage(SUBJECT_FIELD),
				fIncoming, false, B_FOLLOW_LEFT_RIGHT);
	AddChild(fSubject);
	(msg = new BMessage(FIELD_CHANGED))->AddInt32("bitmask", FIELD_SUBJECT);
	fSubject->SetModificationMessage(msg);
	
	if (fResending)
		fSubject->SetEnabled(false);
	
	--y;
	if (!fIncoming)
	{
		r.Set(x - 11, y, CC_FIELD_H + CC_FIELD_WIDTH, y + CC_FIELD_HEIGHT);
		fCc = new TTextControl(r, "", new BMessage(CC_FIELD), fIncoming, false);
		fCc->SetChoiceList(&fEmailList);
		fCc->SetAutoComplete(true);
		AddChild(fCc);
		(msg = new BMessage(FIELD_CHANGED))->AddInt32("bitmask", FIELD_CC);
		fCc->SetModificationMessage(msg);
	
		r.right = r.left + 9;
		r.left = r.right - be_plain_font->StringWidth(CC_TEXT) - 30;
		r.top -= 1;
		fCcMenu = new QPopupMenu(CC_TEXT);
		field = new BMenuField(r, "", "", fCcMenu, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);

		field->SetDivider(0.0);
		field->SetEnabled(true);
		AddChild(field);

		r.Set(BCC_FIELD_H + be_plain_font->StringWidth(BCC_TEXT), y,
			  windowRect.Width() - SEPARATOR_MARGIN, y + BCC_FIELD_HEIGHT);
		y += FIELD_HEIGHT;
		fBcc = new TTextControl(r, "", new BMessage(BCC_FIELD),
						fIncoming, false, B_FOLLOW_LEFT_RIGHT);
		fBcc->SetChoiceList(&fEmailList);
		fBcc->SetAutoComplete(true);
		AddChild(fBcc);
		(msg = new BMessage(FIELD_CHANGED))->AddInt32("bitmask", FIELD_BCC);
		fBcc->SetModificationMessage(msg);
		
		r.right = r.left + 9;
		r.left = r.right - be_plain_font->StringWidth(BCC_TEXT) - 30;
		r.top -= 1;
		fBccMenu = new QPopupMenu(BCC_TEXT);
		field = new BMenuField(r, "", "", fBccMenu, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
		field->SetDivider(0.0);
		field->SetEnabled(true);
		AddChild(field);
	}
	else
	{
		r.Set(x - font.StringWidth(kDateLabel) - 10, y + 4,
			  windowRect.Width(), y + TO_FIELD_HEIGHT + 1);
		y += TO_FIELD_HEIGHT + 5;
		fDate = new BStringView(r, "", "");
		AddChild(fDate);	
		fDate->SetFont(&font);	
		fDate->SetHighColor(0, 0, 0);

		LoadMessage(mail);
	}
	ResizeTo(Bounds().Width(),y);
}


void 
THeaderView::InitEmailCompletion()
{
	// get boot volume
	BVolume volume;
	BVolumeRoster().GetBootVolume(&volume);

	BQuery query;
	query.SetVolume(&volume);
	query.SetPredicate("META:email=**");
	query.Fetch();
	entry_ref ref;

	// The leash is a work-around for when the something in the query breaks
	// and prevents GetNextRef() from ever returning B_ENTRY_NOT_FOUND
	for (int32 leash = 0; query.GetNextRef(&ref) == B_OK && leash < 1024; leash++)
	{
		BNode file;
		if (file.SetTo(&ref) == B_OK)
		{
			BString email;
			if (file.ReadAttrString("META:email", &email) >= B_OK)
				fEmailList.AddChoice(email.String());

			// support for 3rd-party People apps
			for (int16 i = 2;i < 6;i++)
			{
				char attr[16];
				sprintf(attr,"META:email%d",i);
				if (file.ReadAttrString(attr,&email) >= B_OK)
					fEmailList.AddChoice(email.String());
			}
		}
	}
}


void 
THeaderView::InitGroupCompletion()
{
	// get boot volume
	BVolume volume;
	BVolumeRoster().GetBootVolume(&volume);

	// build a list of all unique groups and the addresses
	// they expand to
	BQuery query;
	query.SetVolume(&volume);
	query.SetPredicate("META:group=**");
	query.Fetch();

	map<BString *, BString *, evil> group_map;
	entry_ref ref;
	BNode file;
	while (query.GetNextRef(&ref) == B_OK)
	{
		if (file.SetTo(&ref) != B_OK)
			continue;

		BString groups;
		if (ReadAttrString(&file, "META:group", &groups) < B_OK || groups.Length() == 0)
			continue;

		BString address;
		ReadAttrString(&file, "META:email", &address);

		// avoid adding an empty address
		if (address.Length() == 0)
			continue;

		char *grp = groups.LockBuffer(groups.Length());
		char *next = strchr(grp, ',');

		for (;;)
		{
			if (next) *next = 0;
			while (*grp && *grp == ' ') grp++;

			BString *group = new BString(grp);

			// nobody is in this group yet, start it off
			if (group_map[group] == NULL)
			{
				BString *addresses = new BString(*group);
				addresses->Append(" <");
				addresses->Append(address);
				group_map[group] = addresses;
			}
			else
			{
				BString *a = group_map[group];
				a->Append(", ");
				a->Append(address);
				
				delete group;
			}

			if (!next)
				break;
	
			grp = next+1;
			next = strchr(grp, ',');
		}
	}
	
	map<BString *, BString *, evil>::iterator iter;
	for (iter = group_map.begin(); iter != group_map.end();)
	{
		BString *grp = iter->first;
		BString *addr = iter->second;
		addr->Append(">");
		fEmailList.AddChoice(addr->String());
		++iter;
		group_map.erase(grp);
		delete grp;
		group_map.erase(addr);
		delete addr;
	}

}


void
THeaderView::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case B_SIMPLE_DATA:
		{
			BTextView *textView = dynamic_cast<BTextView *>(Window()->CurrentFocus());
			if (dynamic_cast<TTextControl *>(textView->Parent()) != NULL)
				textView->Parent()->MessageReceived(msg);
			else
			{
				BMessage message(*msg);
				message.what = REFS_RECEIVED;
				Window()->PostMessage(&message, Window());
			}
			break;
		}

		case kMsgFrom:
		{
			BMenuItem *item;
			if (msg->FindPointer("source", (void **)&item) >= B_OK)
				item->SetMarked(true);

			uint32 chain;
			if (msg->FindInt32("id",(int32 *)&chain) >= B_OK)
				fChain = chain;
			break;
		}
	}
}


void
THeaderView::AttachedToWindow(void)
{
	if (fToMenu)
	{
		fToMenu->SetTargetForItems(fTo);
		fToMenu->SetPredicate("META:email=**");
	}
	if (fCcMenu)
	{
		fCcMenu->SetTargetForItems(fCc);
		fCcMenu->SetPredicate("META:email=**");
	}
	if (fBccMenu)
	{
		fBccMenu->SetTargetForItems(fBcc);
		fBccMenu->SetPredicate("META:email=**");
	}
	if (fTo)
		fTo->SetTarget(Looper());
	if (fSubject)
		fSubject->SetTarget(Looper());
	if (fCc)
		fCc->SetTarget(Looper());
	if (fBcc)
		fBcc->SetTarget(Looper());
	if (fAccount)
		fAccount->SetTarget(Looper());
	if (fAccountMenu)
		fAccountMenu->SetTargetForItems(this);

	BBox::AttachedToWindow();
}


void
THeaderView::BuildMenus()
{
	/*
	BMenuItem *item;
	while (item = fToMenu->ItemAt(0)) {
		fToMenu->RemoveItem(item);
		delete item;
	}
	if (!fResending) {
		while (item = fCcMenu->ItemAt(0)) {
			fCcMenu->RemoveItem(item);
			delete item;
		}
		while (item = fBccMenu->ItemAt(0)) {
			fBccMenu->RemoveItem(item);
			delete item;
		}
	}

	// do a query to find all People files where there is a group
	BVolumeRoster	volume;
	BVolume			vol;
	volume.GetBootVolume(&vol);

	BQuery			query;
	query.SetVolume(&vol);
	query.SetPredicate("META:email=**");
	//query.PushAttr("META:email");
	//query.PushString("**");
	//query.PushOp(B_EQ);
	query.Fetch();

	BEntry	entry;
	entry_ref ref;
	BFile	file;
	int		groups=0;
	
	status_t status;
	
	while ((status = query.GetNextRef(&ref)) == B_OK) {
		entry.GetRef(&ref);
		file.SetTo(&ref, B_READ_ONLY);
		if (file.InitCheck() == B_NO_ERROR) {
			BString	name;
			FetchAttribute("META:name", file, name);
			BString email;
			FetchAttribute("META:email", file, email);
			BString grouplist;
			FetchAttribute("META:group", file, grouplist);
			
			BString label;
			BString address;
			// if we have no Name, just use the email address 
			if (name.Length() == 0) {
				address = email;
				label = email;
			// otherwise, pretty-format it 
			} else {
				address.SetTo("") << "\"" << name << "\" <" << email << ">";
				label.SetTo("") << name << " (" << email << ")";
			}	

			if (grouplist.Length() == 0) {
			// just put it in the top level (contains no groups)  
				BMessage msg;
				msg.AddString("address", address.String());
				
				int index=groups;
				while (item = fToMenu->ItemAt(index)) {
					if (strcasecmp(item->Label(), label.String()) > 0) break;
					index++;
				}
				msg.what = M_TO_MENU;
				fToMenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
				
				if (!fResending) {
					msg.what = M_CC_MENU;
					fCcMenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
					
					msg.what = M_BCC_MENU;
					fBccMenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
				}				
			} else {
				
			// this name is in one or more groups 
			grouplist.Append(","); 
			BString group;
			int group_end=0;
			
			while(grouplist.Length() > 0) {
				grouplist.CopyInto(group, 0, group_end = grouplist.FindFirst(",", 0));
				grouplist.Remove(0, group_end +1);

				// here we check to see if the submenu has been created
				if ((item = fToMenu->FindItem(group.String())) == NULL) {
					int index=0;
					item = fToMenu->ItemAt(index);
					while (item != NULL && index < groups) {
						if (strcasecmp(item->Label(), group.String()) > 0) break;
						index++;
						item = fToMenu->ItemAt(index);
					}
					
					BMessage msg;
					msg.AddString("group", group.String());
									
					BMenu *submenu = new BMenu(group.String());
					submenu->SetFont(be_plain_font);
					msg.what = M_TO_MENU;
					fToMenu->AddItem(new BMenuItem(submenu, new BMessage(msg)), index);
					
					if (!fResending) {
						submenu = new BMenu(group.String());
						submenu->SetFont(be_plain_font);
						msg.what = M_CC_MENU;
						fCcMenu->AddItem(new BMenuItem(submenu, new BMessage(msg)), index);
		
						submenu = new BMenu(group.String());
						submenu->SetFont(be_plain_font);
						msg.what = M_BCC_MENU;
						fBccMenu->AddItem(new BMenuItem(submenu, new BMessage(msg)), index);
					}
					groups++;
				}
					
					// the submenu either exists or was just created 
					item = fToMenu->FindItem(group.String());
					BMenu *submenu = item->Submenu();
		
					int index=0;
					while (item = submenu->ItemAt(index)) {
						if (strcasecmp(item->Label(), label.String()) > 0) break;
						index++;
					}
					
					BMessage msg;
					msg.AddString("address", address.String());
					
					msg.what=M_TO_MENU;
					submenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
					
					if (!fResending) {
						msg.what=M_CC_MENU;
						item = fCcMenu->FindItem(group.String());
						submenu = item->Submenu();
						submenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
						
						msg.what=M_BCC_MENU;
						item = fBccMenu->FindItem(group.String());
						submenu = item->Submenu();
						submenu->AddItem(new BMenuItem(label.String(), new BMessage(msg)), index);
					}
				}
			}
		}
	}
	// add the seperator bars
	if (groups > 0) {
		fToMenu->AddItem(new BSeparatorItem(), groups);
		if (!fResending) {
			fCcMenu->AddItem(new BSeparatorItem(), groups);
			fBccMenu->AddItem(new BSeparatorItem(),  groups);
		}
	}
	*/
}


void
THeaderView::SetAddress(BMessage *msg)
{
	const char *msgStr = NULL;
	BTextView *text = NULL;
	bool group = false;

	switch (msg->what)
	{
		case M_TO_MENU:
			text = fTo->TextView();
			if (msg->HasString("group"))
			{
				msg->FindString("group", &msgStr);
				group = true;
			}
			else
				msg->FindString("address", &msgStr);
			break;
		case M_CC_MENU:
			text = fCc->TextView();
			if (msg->HasString("group"))
			{
				msg->FindString("group", &msgStr);
				group = true;
			}
			else
				msg->FindString("address", &msgStr);
			break;
		case M_BCC_MENU:
			text = fBcc->TextView();
			if (msg->HasString("group"))
			{
				msg->FindString("group", &msgStr);
				group = true;
			}
			else
				msg->FindString("address", &msgStr);
			break;
	}

	int32 start, end;
	text->GetSelection(&start, &end);
	if (start != end)
		text->Delete();

	if (group)
	{
		// get boot volume
		BVolume volume;
		BVolumeRoster().GetBootVolume(&volume);

		BQuery query;
		query.SetVolume(&volume);

		BString predicate;
		predicate << "META:group='*" << msgStr << "'";
		query.SetPredicate(predicate.String());
		query.Fetch();

		BEntry entry;
		while (query.GetNextEntry(&entry) == B_NO_ERROR)
		{
			BFile file;
			file.SetTo(&entry, O_RDONLY);
			BString	name;
			ReadAttrString(&file, "META:name", &name);
			BString email;
			ReadAttrString(&file, "META:email", &email);

			BString address;
			/* if we have no Name, just use the email address */
			if (name.Length() == 0)
				address = email;
			else
				/* otherwise, pretty-format it */
				address.SetTo("") << "\"" << name << "\" <" << email << ">";
			
			if ((end = text->TextLength()) != 0)
			{
				text->Select(end, end);
				text->Insert(", ");
			}	
			text->Insert(address.String());
		}
	}
	else
	{
		if ((end = text->TextLength()) != 0)
		{
			text->Select(end, end);
			text->Insert(", ");
		}
		text->Insert(msgStr);
	}

	text->Select(text->TextLength(), text->TextLength());
}


status_t
THeaderView::LoadMessage(Mail::Message *mail)
{
	//
	//	Set the date on this message
	//
	const char *dateField = mail->Date();
	char string[256];
	sprintf(string, "%s   %s", kDateLabel, dateField != NULL ? dateField : "Unknown");
	fDate->SetText(string);

	//	
	//	Set contents of header fields
	//
	if (fIncoming && !fResending)
	{
		if (fBcc != NULL)
			fBcc->SetEnabled(false);

		if (fCc != NULL)
			fCc->SetEnabled(false);

		if (fAccount != NULL)
			fAccount->SetEnabled(false);

		if (fAccountTo != NULL)
			fAccountTo->SetEnabled(false);

		fSubject->SetEnabled(false);
		fTo->SetEnabled(false);
	}

	//	Set Subject: & From: fields
	fSubject->SetText(mail->Subject());
	fTo->SetText(mail->From());

	//	Set Account/To Field
	if (fAccountTo != NULL)
		fAccountTo->SetText(mail->To());

	if (fAccount != NULL && mail->GetAccountName(string,sizeof(string)) == B_OK)
		fAccount->SetText(string);

	return B_OK;
}


//====================================================================
//	#pragma mark -


TTextControl::TTextControl(BRect rect, char *label, BMessage *msg, 
	bool incoming, bool resending, int32 resizingMode)
	:	BComboBox(rect, "happy", label, msg, resizingMode)
	//:BTextControl(rect, "happy", label, "", msg, resizingMode)
{
	strcpy(fLabel, label);
	fCommand = msg != NULL ? msg->what : 0UL;
	fIncoming = incoming;
	fResending = resending;
}


void
TTextControl::AttachedToWindow()
{
	BFont font = *be_plain_font;
	BTextView *text;

	SetHighColor(0, 0, 0);
	// BTextControl::AttachedToWindow();
	BComboBox::AttachedToWindow();
	font.SetSize(FONT_SIZE);
	SetFont(&font);

	SetDivider(StringWidth(fLabel) + 6);
	text = (BTextView *)ChildAt(0);
	text->SetFont(&font);
}


void
TTextControl::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case B_SIMPLE_DATA:
			if (!fIncoming || fResending)
			{
				BMessage message(REFS_RECEIVED);
				bool enclosure = false;

				entry_ref ref;
				for (int32 index = 0;msg->FindRef("refs", index++, &ref) == B_OK;)
				{
					BFile file(&ref, O_RDONLY);
					if (file.InitCheck() == B_NO_ERROR)
					{
						BNodeInfo node(&file);
						char type[B_FILE_NAME_LENGTH];
						node.GetType(type);

						if (fCommand != SUBJECT_FIELD && !strcmp(type,"application/x-person"))
						{
							// add person's E-mail address to the To: field

							const char *attr;
							if (msg->FindString("attr", &attr) < B_OK)
								attr = "META:email";

							BString email;
							ReadAttrString(&file,attr,&email);

							/* we got something... */	
							if (email.Length() > 0)
							{
								/* see if we can get a username as well */
								BString name;
								ReadAttrString(&file,"META:name",&name);
									
								BString	address;
								/* if we have no Name, just use the email address */
								if (name.Length() == 0)
									address = email;
								else
								{
									/* otherwise, pretty-format it */
									address << "\"" << name << "\" <" << email << ">";
								}
								
								if (int end = TextView()->TextLength())
								{
									TextView()->Select(end, end);
									TextView()->Insert(", ");
								}	
								TextView()->Insert(address.String());
							}
						}
						else
						{
							enclosure = true;
							message.AddRef("refs", &ref);
						}
					}
				}
				if (enclosure)
					Window()->PostMessage(&message, Window());
			}
			break;

		case M_SELECT:
		{
			BTextView *textView = (BTextView *)ChildAt(0);
			if (textView != NULL)
				textView->Select(0, textView->TextLength());
			break;
		}

		default:
			// BTextControl::MessageReceived(msg);
			BComboBox::MessageReceived(msg);
	}
}


bool
TTextControl::HasFocus()
{
	BTextView *textView = TextView();

	return textView->IsFocus();
}


//====================================================================
//	QPopupMenu (definition at the beginning of this file)
//	#pragma mark -


QPopupMenu::QPopupMenu(const char *title)
	:	QueryMenu(title, true),
		fGroups(0)
{
}


void
QPopupMenu::AddPersonItem(const entry_ref *ref, ino_t node, BString &name, BString &email,
	const char *attr, BMenu *groupMenu, BMenuItem *superItem)
{
	BString label;
	
	// if we have no Name, just use the email address 
	if (name.Length() == 0)
		label = email;
	else
		// otherwise, pretty-format it 
		label << name << " (" << email << ")";

	BMessage *msg = new BMessage(B_SIMPLE_DATA);
	msg->AddRef("refs", ref);
	msg->AddInt64("node", node);
	if (attr)
		msg->AddString("attr", attr);

	BMenuItem *item = new BMenuItem(label.String(), msg);
	if (fTargetHandler)
		item->SetTarget(fTargetHandler);

	// If no group, just add it; else add it to group menu
	if (!groupMenu)
		AddItem(item);
	else
	{
		groupMenu->AddItem(item);
		// Add ref to group super item
		BMessage *superMsg = superItem->Message();
		superMsg->AddRef("refs", ref);
	}
}


void
QPopupMenu::EntryCreated(const entry_ref &ref, ino_t node)
{
	BNode file;
	if (file.SetTo(&ref) < B_OK)
		return;

	BMessage 	*msg;
	BMenu		*groupMenu;
	BMenuItem	*superItem = NULL;
	int32 		items = CountItems();

	if (!items)
		AddSeparatorItem();
	
	// Does the file have a group attribute
	BString groups;
	ReadAttrString(&file,"META:group",&groups);
	
	do
	{
		// split comma separated string
		BString group;
		int32 comma;
		if ((comma = groups.FindFirst(',')) > 0)
		{
			groups.MoveInto(group,0,comma);
			groups.Remove(0,1);
		}
		else
			group.Adopt(groups);
		
		// trim white spaces
		int32 i = 0;
		for (i = 0;isspace(group.ByteAt(i));i++) {}
		if (i)
			group.Remove(0,i);
		for (i = group.Length() - 1;isspace(group.ByteAt(i));i--) {}
		group.Truncate(i + 1);

		groupMenu = NULL;

		if (group.Length() > 0)
		{
			BMenu *sub;
	
			// Look for submenu with label == group name
			for (int32 i = 0; i < items; i++)
			{
				if ((sub = SubmenuAt(i)) != NULL)
				{
					superItem = sub->Superitem();
					if (!strcmp(superItem->Label(), group.String()))
					{
						groupMenu = sub;
						i++;
						break;
					}
				}
			}
			
			// If no submenu, create one
			if (!groupMenu)
			{
				// Find where it should go (alphabetical)
				int32 mindex = 0;
				for (; mindex < fGroups; mindex++)
				{
					if (strcmp(ItemAt(mindex)->Label(), group.String()) > 0)
						break;
				}
				
				groupMenu = new BMenu(group.String());
				groupMenu->SetFont(be_plain_font);
				AddItem(groupMenu, mindex);
				superItem = groupMenu->Superitem();
				superItem->SetMessage(new BMessage(B_SIMPLE_DATA));
				if (fTargetHandler)
					superItem->SetTarget(fTargetHandler);
	
				fGroups++;
			}
		}
		
		msg = new BMessage(B_SIMPLE_DATA);
		msg->AddRef("refs", &ref);
		msg->AddInt64("node", node);
		
		BString	name;
		ReadAttrString(&file,"META:name",&name);

		BString email;
		ReadAttrString(&file,"META:email",&email);

		if (email.Length() != 0 || name.Length() != 0)
			AddPersonItem(&ref, node, name, email, NULL, groupMenu, superItem);

		// support for 3rd-party People apps
		for (int16 i = 2;i < 6;i++)
		{
			char attr[16];
			sprintf(attr,"META:email%d",i);
			if (ReadAttrString(&file, attr, &email) >= B_OK && email.Length() > 0)
				AddPersonItem(&ref, node, name, email, attr, groupMenu, superItem);
		}
	} while (groups.Length() > 0);
}


void
QPopupMenu::EntryRemoved(ino_t /*node*/)
{
}

