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
//	Content.cpp
//	
//--------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <Alert.h>
#include <Beep.h>
#include <E-mail.h>
#include <Beep.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <Mime.h>
#include <Beep.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <Region.h>
#include <Roster.h>
#include <ScrollView.h>
#include <TextView.h>
#include <UTF8.h>
#include <MenuItem.h>
#include <Roster.h>

#include <MailMessage.h>
#include <MailAttachment.h>
#include <base64.h>

#include "Mail.h"
#include "Content.h"
#include "Utilities.h"
#include "FieldMsg.h"
#include "Words.h"

extern	bool	header_flag;
extern	uint32	gMailEncoding;


inline bool
IsInitialUTF8Byte(uchar b)	
	{ return ((b & 0xC0) != 0x80); }
	

//====================================================================
//	#pragma mark -


TContentView::TContentView(BRect rect, bool incoming, BFile *file, BFont *font)
			 :BView(rect, "m_content", B_FOLLOW_ALL, B_WILL_DRAW |
													B_FULL_UPDATE_ON_RESIZE),
			fFocus(false),
			fIncoming(incoming)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BFont v_font = *be_plain_font;
	v_font.SetSize(FONT_SIZE);
	fOffset = 12;

	BRect r(rect);
	r.OffsetTo(0,0);
	r.right -= B_V_SCROLL_BAR_WIDTH;
	r.bottom -= B_H_SCROLL_BAR_HEIGHT;
	r.top += 4;
	BRect text(r);
	text.OffsetTo(0,0);
	text.InsetBy(5, 5);

	fTextView = new TTextView(r, text, fIncoming, file, this, font);
	BScrollView *scroll = new BScrollView("", fTextView, B_FOLLOW_ALL, 0, true, 
	  true);
	AddChild(scroll);
}


void TContentView::MessageReceived(BMessage *msg)
{
	char		*str;
	char		*quote;
	const char	*text;
	char		new_line = '\n';
	int32		finish;
	int32		len;
	int32		loop;
	int32		new_start;
	int32		offset;
	int32		removed = 0;
	int32		start;
	BFile		file;
	BFont		*font;
	BRect		r;
	entry_ref	ref;
	off_t		size;

	switch (msg->what) {
		case CHANGE_FONT:
			msg->FindPointer("font", (void **)&font);
			fTextView->SetFontAndColor(0, LONG_MAX, font);
			fTextView->Invalidate(Bounds());
			break;

		case M_QUOTE:
			r = fTextView->Bounds();
			fTextView->GetSelection(&start, &finish);
			quote = (char *)malloc(strlen(QUOTE) + 1);
			strcpy(quote, QUOTE);
			len = strlen(QUOTE);
			fTextView->GoToLine(fTextView->CurrentLine());
			fTextView->GetSelection(&new_start, &new_start);
			fTextView->Select(new_start, finish);
			finish -= new_start;
			str = (char *)malloc(finish + 1);
			fTextView->GetText(new_start, finish, str);
			offset = 0;
			for (loop = 0; loop < finish; loop++) {
				if (str[loop] == '\n') {
					quote = (char *)realloc(quote, len + loop - offset + 1);
					memcpy(&quote[len], &str[offset], loop - offset + 1);
					len += loop - offset + 1;
					offset = loop + 1;
					if (offset < finish) {
						quote = (char *)realloc(quote, len + strlen(QUOTE));
						memcpy(&quote[len], QUOTE, strlen(QUOTE));
						len += strlen(QUOTE);
					}
				}
			}
			if (offset != finish) {
				quote = (char *)realloc(quote, len + (finish - offset));
				memcpy(&quote[len], &str[offset], finish - offset);
				len += finish - offset;
			}
			free(str);

			fTextView->Delete();
			fTextView->Insert(quote, len);
			if (start != new_start) {
				start += strlen(QUOTE);
				len -= (start - new_start);
			}
			fTextView->Select(start, start + len);
			fTextView->ScrollTo(r.LeftTop());
			free(quote);
			break;

		case M_REMOVE_QUOTE:
			r = fTextView->Bounds();
			fTextView->GetSelection(&start, &finish);
			len = start;
			fTextView->GoToLine(fTextView->CurrentLine());
			fTextView->GetSelection(&start, &start);
			fTextView->Select(start, finish);
			new_start = finish;
			finish -= start;
			str = (char *)malloc(finish + 1);
			fTextView->GetText(start, finish, str);
			for (loop = 0; loop < finish; loop++) {
				if (strncmp(&str[loop], QUOTE, strlen(QUOTE)) == 0) {
					finish -= strlen(QUOTE);
					memcpy(&str[loop], &str[loop + strlen(QUOTE)],
									finish - loop);
					removed += strlen(QUOTE);
				}
				while ((loop < finish) && (str[loop] != '\n')) {
					loop++;
				}
				if (loop == finish)
					break;
			}
			if (removed) {
				fTextView->Delete();
				fTextView->Insert(str, finish);
				new_start -= removed;
				fTextView->Select(new_start - finish + (len - start) - 1,
								  new_start);
			}
			else
				fTextView->Select(len, new_start);
			fTextView->ScrollTo(r.LeftTop());
			free(str);
			break;

		case M_SIGNATURE:
			msg->FindRef("ref", &ref);
			file.SetTo(&ref, O_RDWR);
			if (file.InitCheck() == B_NO_ERROR) {
				file.GetSize(&size);
				str = (char *)malloc(size);
				size = file.Read(str, size);
				fTextView->GetSelection(&start, &finish);
				text = fTextView->Text();
				len = fTextView->TextLength();
				if ((len) && (text[len - 1] != '\n')) {
					fTextView->Select(len, len);
					fTextView->Insert(&new_line, 1);
					len++;
				}
				fTextView->Select(len, len);
				fTextView->Insert(str, size);
				fTextView->Select(len, len + size);
				fTextView->ScrollToSelection();
				fTextView->Select(start, finish);
				fTextView->ScrollToSelection();
			} else {
				beep();
				(new BAlert("", "An error occurred trying to open this signature.",
					"Sorry"))->Go();
			}
			break;

		case M_FIND:
			FindString(msg->FindString("findthis"));
			break;


		default:
			BView::MessageReceived(msg);
	}
}


void TContentView::FindString(const char *str)
{
	int32	finish;
	int32	pass = 0;
	int32	start = 0;

	if (str == NULL)
		return;

	//	
	//	Start from current selection or from the beginning of the pool
	//	
	const char *text = fTextView->Text();
	int32 count = fTextView->TextLength();
	fTextView->GetSelection(&start, &finish);
	if (start != finish)
		start = finish;
	if (!count || text == NULL)
		return;

	//	
	//	Do the find
	//
	while (pass < 2) {
		long found = -1;
		char lc = tolower(str[0]);
		char uc = toupper(str[0]);
		for (long i = start; i < count; i++) {
			if (text[i] == lc || text[i] == uc) {
				const char *s = str;
				const char *t = text + i;
				while (*s && (tolower(*s) == tolower(*t))) {
					s++;
					t++;
				}
				if (*s == 0) {
					found = i;
					break;
				}
			}
		}
	
		//	
		//	Select the text if it worked
		//	
		if (found != -1) {
			Window()->Activate();
			fTextView->Select(found, found + strlen(str));
			fTextView->ScrollToSelection();
			fTextView->MakeFocus(true);
			return;
		}
		else if (start) {
			start = 0;
			text = fTextView->Text();
			count = fTextView->TextLength();
			pass++;
		} else {
			beep();
			return;
		}
	}
}


void TContentView::Focus(bool focus)
{
	if (fFocus != focus) {
		fFocus = focus;
		Draw(Frame());
	}
}


void TContentView::FrameResized(float /* width */, float /* height */)
{
	BFont v_font = *be_plain_font;
	v_font.SetSize(FONT_SIZE);

	font_height fHeight;
	v_font.GetHeight(&fHeight);

	BRect r(fTextView->Bounds());
	r.OffsetTo(0, 0);
	r.InsetBy(5, 5);	
	fTextView->SetTextRect(r);
}


//====================================================================
//	#pragma mark -


TTextView::TTextView(BRect frame, BRect text, bool incoming, BFile *file,
                      TContentView *view, BFont *font)
          :BTextView(frame, "", text, B_FOLLOW_ALL, B_WILL_DRAW | B_NAVIGABLE),
			fHeader(header_flag),
			fReady(false),
			fYankBuffer(NULL),
			fLastPosition(-1),
			fFile(file),
			fMail(NULL),
			fFont(font),
			fParent(view),
			fThread(0),
			fPanel(NULL),
			fIncoming(incoming),
			fSpellCheck(false),
			fRaw(false),
			fCursor(false)
{
	BFont	menuFont = *be_plain_font;
	menuFont.SetSize(10);

	fStopSem = create_sem(1, "reader_sem");
	if (fIncoming)
		SetStylable(true);

	fEnclosures = new BList();

	//
	//	Enclosure pop up menu
	//
	fEnclosureMenu = new BPopUpMenu("Enclosure", false, false);
	fEnclosureMenu->SetFont(&menuFont);
	fEnclosureMenu->AddItem(new BMenuItem("Save Enclosure"B_UTF8_ELLIPSIS, 
	  new BMessage(M_SAVE)));
	fEnclosureMenu->AddItem(new BMenuItem("Open Enclosure", new 
	  BMessage(M_ADD)));

	//
	//	Hyperlink pop up menu
	//
	fLinkMenu = new BPopUpMenu("Link", false, false);
	fLinkMenu->SetFont(&menuFont);
	fLinkMenu->AddItem(new BMenuItem("Open This Link", new BMessage(M_ADD)));

	SetDoesUndo(true);
}


TTextView::~TTextView()
{
	ClearList();
	delete fPanel;

	if (fYankBuffer)
		free(fYankBuffer);

	delete_sem(fStopSem);

	delete fMail;
}


void TTextView::AttachedToWindow()
{
	BTextView::AttachedToWindow();
	fFont.SetSpacing(B_FIXED_SPACING);
	SetFontAndColor(&fFont);
	if (fFile) {
		LoadMessage(fFile, false, false, NULL);
		if (fIncoming)
			MakeEditable(false);
	}
}


void TTextView::KeyDown(const char *key, int32 count)
{
	char		new_line = '\n';
	char		raw;
	int32		end;
	int32 		start;
	uint32		mods;
	BMessage	*msg;
	int32		textLen = TextLength();

	msg = Window()->CurrentMessage();
	mods = msg->FindInt32("modifiers");

	switch (key[0]) {
		case B_HOME:
			if (IsSelectable()) {
				if (IsEditable())
					GoToLine(CurrentLine());
				else {
					Select(0, 0);
					ScrollToSelection();
				}
			}
			break;

		case B_END:
			if (IsSelectable()) {
				if (CurrentLine() == CountLines() - 1)
					Select(TextLength(), TextLength());
				else {
					GoToLine(CurrentLine() + 1);
					GetSelection(&start, &end);
					Select(start - 1, start - 1);
				}
			}
			break;


		case 0x02:						// ^b - back 1 char
			if (IsSelectable()) {
				GetSelection(&start, &end);
				while (!IsInitialUTF8Byte(ByteAt(--start))) {
					if (start < 0) {
						start = 0;
						break;
					}
				}
				if (start >= 0) {
					Select(start, start);
					ScrollToSelection();
				}
			}
			break;

		case B_DELETE:
			if (IsSelectable()) {
				if ((key[0] == B_DELETE) || (mods & B_CONTROL_KEY)) {	// ^d
					if (IsEditable()) {
						GetSelection(&start, &end);
						if (start != end)
							Delete();
						else {
							for (end = start + 1; !IsInitialUTF8Byte(ByteAt(end));
									end++) {
								if (end > textLen) {
									end = textLen;
									break;
								}
							}
							Select(start, end);
							Delete();
						}
					}
				}
				else
					Select(textLen, textLen);
				ScrollToSelection();
			}
			break;

		case 0x05:						// ^e - end of line
			if ((IsSelectable()) && (mods & B_CONTROL_KEY)) {
				if (CurrentLine() == CountLines() - 1)
					Select(TextLength(), TextLength());
				else {
					GoToLine(CurrentLine() + 1);
					GetSelection(&start, &end);
					Select(start - 1, start - 1);
				}
			}
			break;

		case 0x06:						// ^f - forward 1 char
			if (IsSelectable()) {
				GetSelection(&start, &end);
				if (end > start)
					start = end;
				else {
					for (end = start + 1; !IsInitialUTF8Byte(ByteAt(end)); end++) {
						if (end > textLen) {
							end = textLen;
							break;
						}
					}	
					start = end;			
				}
				Select(start, start);
				ScrollToSelection();
			}
			break;

		case 0x0e:						// ^n - next line
			if (IsSelectable()) {
				raw = B_DOWN_ARROW;
				BTextView::KeyDown(&raw, 1);
			}
			break;

		case 0x0f:						// ^o - open line
			if (IsEditable()) {
				GetSelection(&start, &end);
				Delete();
				Insert(&new_line, 1);
				Select(start, start);
				ScrollToSelection();
			}
			break;

		case B_PAGE_UP:
			if (mods & B_CONTROL_KEY) {	// ^k kill text from cursor to e-o-line
				if (IsEditable()) {
					GetSelection(&start, &end);
					if ((start != fLastPosition) && (fYankBuffer)) {
						free(fYankBuffer);
						fYankBuffer = NULL;
					}
					fLastPosition = start;
					if (CurrentLine() < (CountLines() - 1)) {
						GoToLine(CurrentLine() + 1);
						GetSelection(&end, &end);
						end--;
					}
					else
						end = TextLength();
					if (end < start)
						break;
					if (start == end)
						end++;
					Select(start, end);
					if (fYankBuffer) {
						fYankBuffer = (char *)realloc(fYankBuffer,
									 strlen(fYankBuffer) + (end - start) + 1);
						GetText(start, end - start,
							    &fYankBuffer[strlen(fYankBuffer)]);
					} else {
						fYankBuffer = (char *)malloc(end - start + 1);
						GetText(start, end - start, fYankBuffer);
					}
					Delete();
					ScrollToSelection();
				}
				break;
			}
			
			BTextView::KeyDown(key, count);
			break;

		case 0x10:						// ^p goto previous line
			if (IsSelectable()) {
				raw = B_UP_ARROW;
				BTextView::KeyDown(&raw, 1);
			}
			break;

		case 0x19:						// ^y yank text
			if ((IsEditable()) && (fYankBuffer)) {
				Delete();
				Insert(fYankBuffer);
				ScrollToSelection();
			}
			break;

		default:
			BTextView::KeyDown(key, count);
	}
}


void TTextView::MakeFocus(bool focus)
{
    BTextView::MakeFocus(focus);
	fParent->Focus(focus);
}


void TTextView::MessageReceived(BMessage *msg)
{
	bool		inserted = false;
	bool		is_enclosure = false;
	const char	*name;
	char		type[B_FILE_NAME_LENGTH];
	char		*text;
	int32		end;
	int32		index = 0;
	int32		loop;
	int32		offset;
	int32		opcode;
	int32		start;
	BFile		file;
	BMessage	message(REFS_RECEIVED);
	BNodeInfo	*node;
	entry_ref	ref;
	dev_t		device;
	ino_t		inode;
	off_t		len = 0;
	off_t		size;
	hyper_text	*enclosure;

	switch (msg->what) {
		case B_SIMPLE_DATA:
			if (!fIncoming) {
				while (msg->FindRef("refs", index++, &ref) == B_NO_ERROR) {
					file.SetTo(&ref, O_RDONLY);
					if (file.InitCheck() == B_NO_ERROR) {
						node = new BNodeInfo(&file);
						node->GetType(type);
						delete node;
						file.GetSize(&size);
						if ((!strncasecmp(type, "text/", 5)) && (size)) {
							len += size;
							text = (char *)malloc(size);
							file.Read(text, size);
							if (!inserted) {
								GetSelection(&start, &end);
								Delete();
								inserted = true;
							}
							offset = 0;
							for (loop = 0; loop < size; loop++) {
								if (text[loop] == '\n') {
									Insert(&text[offset], loop - offset + 1);
									offset = loop + 1;
								}
								else if (text[loop] == '\r') {
									text[loop] = '\n';
									Insert(&text[offset], loop - offset + 1);
									if ((loop + 1 < size)
											&& (text[loop + 1] == '\n'))
										loop++;
									offset = loop + 1;
								}
							}
							free(text);
						} else {
							is_enclosure = true;
							message.AddRef("refs", &ref);
						}
					}
				}
				if (inserted)
					Select(start, start + len);
				if (is_enclosure)
					Window()->PostMessage(&message, Window());
			}
			break;

		case M_HEADER:
			msg->FindBool("header", &fHeader);
			SetText(NULL);
			LoadMessage(fFile, false, false, NULL);
			break;

		case M_RAW:
			Window()->Unlock();
			StopLoad();
			Window()->Lock();
			msg->FindBool("raw", &fRaw);
			SetText(NULL);
			LoadMessage(fFile, false, false, NULL);
			break;

		case M_SELECT:
			if (IsSelectable())
				Select(0, TextLength());
			break;

		case M_SAVE:
			Save(msg);
			break;

		case B_NODE_MONITOR:
			if (msg->FindInt32("opcode", &opcode) == B_NO_ERROR) {
				msg->FindInt32("device", &device);
				msg->FindInt64("node", &inode);
				while ((enclosure = (hyper_text *)fEnclosures->ItemAt(index++))
						!= NULL) {
					if ((device == enclosure->node.device) &&
						(inode == enclosure->node.node)) {
						if (opcode == B_ENTRY_REMOVED) {
							enclosure->saved = false;
							enclosure->have_ref = false;
						}
						else if (opcode == B_ENTRY_MOVED) {
							enclosure->ref.device = device;
							msg->FindInt64("to directory",
								&enclosure->ref.directory);
							msg->FindString("name", &name);
							enclosure->ref.set_name(name);
						}
						break;
					}
				}
			}
			break;

		// 
		// Tracker has responded to a BMessage that was dragged out of 
		// this email message.  It has created a file for us, we just have to
		// put the stuff in it.  
		//
		case B_COPY_TARGET:
		{
			BMessage data;
			if (msg->FindMessage("be:originator-data", &data) == B_OK)
			{
				entry_ref directory;
				const char *name;
				hyper_text *enclosure;

				if (data.FindPointer("enclosure", (void **)&enclosure) == B_OK 
				  && msg->FindString("name", &name) == B_OK 
				  && msg->FindRef("directory", &directory) == B_OK)
				{
					switch (enclosure->type)
					{
						case TYPE_ENCLOSURE:
						case TYPE_BE_ENCLOSURE:
						{
							//
							//	Enclosure.  Decode the data and write it out.	
							//
							BMessage saveMsg(M_SAVE);
							saveMsg.AddString("name", name);
							saveMsg.AddRef("directory", &directory);
							saveMsg.AddPointer("enclosure", enclosure);	
							Save(&saveMsg, false);
							break;
						}

						case TYPE_URL:
						{
							const char *replyType;
							if (msg->FindString("be:filetypes", &replyType) != B_OK)
								// drag recipient didn't ask for any specific type,
								// create a bookmark file as default
								replyType = "application/x-vnd.Be-bookmark";
							
							BDirectory dir(&directory);	
							BFile file(&dir, name, B_READ_WRITE);
							if (file.InitCheck() == B_OK) {
								
								if (strcmp(replyType,
										"application/x-vnd.Be-bookmark") == 0)
									// we got a request to create a bookmark, stuff
									// it with the url attribute
									file.WriteAttr("META:url", B_STRING_TYPE, 0, 
									  enclosure->name, strlen(enclosure->name) + 1);
								
								else if (strcasecmp(replyType, "text/plain")
										== 0)
									// create a plain text file, stuff it with
									// the url as text
									file.Write(enclosure->name,
										strlen(enclosure->name));

								BNodeInfo fileInfo(&file);
								fileInfo.SetType(replyType);
							}
							break;
						}
		
						case TYPE_MAILTO:
						{
							//
							//	Add some attributes to the already created 
							//	person file.  Strip out the 'mailto:' if 
							//  possible.
							//
							char *addrStart = enclosure->name;
							while (true)
							{
								if (*addrStart == ':') {	
									addrStart++;
									break;
								}
					
								if (*addrStart == '\0') {
									addrStart = enclosure->name;		
									break;
								}
			
								addrStart++;
							}

							const char *replyType;
							if (msg->FindString("be:filetypes", &replyType) != B_OK)
								// drag recipient didn't ask for any specific type,
								// create a bookmark file as default
								replyType = "application/x-vnd.Be-bookmark";
							
							BDirectory dir(&directory);	
							BFile file(&dir, name, B_READ_WRITE);
							if (file.InitCheck() == B_OK) {
								
								if (strcmp(replyType, "application/x-person")
										== 0)
									// we got a request to create a bookmark, stuff
									// it with the address attribute
									file.WriteAttr("META:email", B_STRING_TYPE, 0, 
									  addrStart, strlen(enclosure->name) + 1);
								
								else if (strcasecmp(replyType, "text/plain")
										== 0)
									// create a plain text file, stuff it with the
									// email as text
									file.Write(addrStart, strlen(addrStart));

								BNodeInfo fileInfo(&file);
								fileInfo.SetType(replyType);
							}
							break;
						}
					};
				}
				else
				{
					//
					// Assume this is handled by BTextView...		
					// (Probably drag clipping.)
					//
					BTextView::MessageReceived(msg);
				}
			}

			break;
		}

		default:
			BTextView::MessageReceived(msg);
	}
}


void TTextView::MouseDown(BPoint where)
{
	if (IsEditable())
	{
		BPoint point;
		uint32 buttons;
		GetMouse(&point, &buttons);
		if (gDictCount && (buttons == B_SECONDARY_MOUSE_BUTTON))
		{
			int32 offset, start, end, length;
			const char *text = Text();
			offset = OffsetAt(where);
			if (isalpha(text[offset]))
			{
				length = TextLength();
				
				//Find start and end of word
				//FindSpellBoundry(length, offset, &start, &end);
				
				char c;
				bool isAlpha, isApost, isCap;
				int32 first;
				
				for (first=offset;
					(first >= 0) && (((c=text[first])=='\'')||isalpha(c));
					first--) {} first++;
				isCap = isupper(text[first]);
				
				for (start = offset, c=text[start], isAlpha=isalpha(c),
					isApost=(c=='\''); (start >= 0) && (isAlpha || (isApost
					&& (((c=text[start+1])!='s')||!isCap) && isalpha(c)
					&& isalpha(text[start-1]))); start--, c=text[start],
					isAlpha=isalpha(c), isApost=(c=='\'')){} start++;
				
				for (end = offset, c=text[end], isAlpha=isalpha(c),
					isApost=(c=='\''); (end < length) && (isAlpha || (isApost
					&& (((c=text[end+1])!='s')||!isCap) && isalpha(c))); 
					end++, c=text[end], isAlpha=isalpha(c), isApost=(c=='\'')){}
				
				length = end-start;
				BString srcWord;
				srcWord.SetTo(text+start, length);
				
				bool		foundWord = false;
				BList 		matches;
				BString 	*string;
				
				BMenuItem *menuItem;
				BPopUpMenu menu("Words", false, false);
				
				int32 matchCount;
				for (int32 i = 0; i < gDictCount; i++)
					matchCount = gWords[i]->FindBestMatches(&matches,
						srcWord.String());
				
				if (matches.CountItems())
				{
					sort_word_list(&matches, srcWord.String());
					for (int32 i = 0; (string = (BString *)matches.ItemAt(i)) != NULL; i++)
					{
						menu.AddItem((menuItem = new BMenuItem(string->String(),
							NULL)));
						if (strcasecmp(string->String(), srcWord.String()) == 0) {
							menuItem->SetEnabled(false);
							foundWord = true;
						}
						delete string;
					}
				}
				else
				{
					(menuItem = new BMenuItem("No Matches",
						NULL))->SetEnabled(false);
					menu.AddItem(menuItem);
				}
				
				BMenuItem *addItem = NULL;
				if (!foundWord && (gUserDict >= 0))
				{
					menu.AddSeparatorItem();
					addItem = new BMenuItem("Add", NULL);
					menu.AddItem(addItem);
				}
				
				point = ConvertToScreen(where);
				if ((menuItem = menu.Go(point, false, false)) != NULL)
				{
					if (menuItem == addItem)
					{
						BString newItem(srcWord.String());
						newItem << "\n";
						gWords[gUserDict]->InitIndex();
						gExactWords[gUserDict]->InitIndex();
						gUserDictFile->Write(newItem.String(), newItem.Length());
						gWords[gUserDict]->BuildIndex();
						gExactWords[gUserDict]->BuildIndex();
						if (fSpellCheck)
							CheckSpelling(0, TextLength());
					}
					else
					{
						int32 len = strlen(menuItem->Label());
						Select(start, start);
						Delete(start, end);
						Insert(start, menuItem->Label(), len);
						Select(start+len, start+len);
					}
				}
			}
			return;
		}
		else if (fSpellCheck && IsEditable())
		{
			int32 s, e;
			
			GetSelection(&s, &e);
			FindSpellBoundry(1, s, &s, &e);
			CheckSpelling(s, e);
		}
		
	}
	else	// is not editable, look for enclosures/links
	{
		int32 clickOffset = OffsetAt(where);
		int32 items = fEnclosures->CountItems();
		for (int32 loop = 0; loop < items; loop++)
		{
			hyper_text *enclosure = (hyper_text*) fEnclosures->ItemAt(loop);
			if (clickOffset < enclosure->text_start || clickOffset >= enclosure->text_end)
				continue;

			//
			// The user is clicking on this attachment
			//

			int32 start;
			int32 finish;
			Select(enclosure->text_start, enclosure->text_end);
			GetSelection(&start, &finish);
			Window()->UpdateIfNeeded();

			bool drag = false;
			bool held = false;
			uint32 buttons = 0;
			if (Window()->CurrentMessage()) {
				Window()->CurrentMessage()->FindInt32("buttons", 
				  (int32 *) &buttons);
			}

			//
			// If this is the primary button, wait to see if the user is going 
			// to single click, hold, or drag.
			//
			if (buttons != B_SECONDARY_MOUSE_BUTTON)
			{
				BPoint point = where;
				bigtime_t popupDelay;
				get_click_speed(&popupDelay);
				popupDelay *= 2;
				popupDelay += system_time();
				while (buttons && abs((int)(point.x - where.x)) < 4 && 
				  abs((int)(point.y - where.y)) < 4 && 
				  system_time() < popupDelay)
				{
					snooze(10000);
					GetMouse(&point, &buttons);
				}	

				if (system_time() < popupDelay)
				{
					//
					// The user either dragged this or released the button.
					// check if it was dragged.
					//
					if (!(abs((int)(point.x - where.x)) < 4 && 
						  abs((int)(point.y - where.y)) < 4) && buttons)
					{
						drag = true;
					}
				}
				else 
				{
					//
					//	The user held the button down.
					//
					held = true;
				}
			}

			//
			//	If the user has right clicked on this menu, 
			// 	or held the button down on it for a while, 
			//	pop up a context menu.
			//
			if (buttons == B_SECONDARY_MOUSE_BUTTON || held)
			{
				//
				// Right mouse click... Display a menu
				//
				BPoint point = where;
				ConvertToScreen(&point);

				BMenuItem *item;
				if ((enclosure->type != TYPE_ENCLOSURE) &&
					(enclosure->type != TYPE_BE_ENCLOSURE))
					item = fLinkMenu->Go(point, true);
				else
					item = fEnclosureMenu->Go(point, true);

				if (item) {
					if (item->Message()->what == M_SAVE) {
						if (fPanel)
							fPanel->SetEnclosure(enclosure);
						else {
							fPanel = new TSavePanel(enclosure, this);
							fPanel->Window()->Show();
						}
					}
					else
						Open(enclosure);
				}
			}
			else
			{
				//
				// Left button.  If the user single clicks, open this link.  
				// Otherwise, initiate a drag.
				//
				if (drag) {
					BMessage dragMessage(B_SIMPLE_DATA);
					dragMessage.AddInt32("be:actions", B_COPY_TARGET);
					dragMessage.AddString("be:types", B_FILE_MIME_TYPE);
					switch (enclosure->type)
					{
						case TYPE_BE_ENCLOSURE:
						case TYPE_ENCLOSURE:
							//
							// Attachment.  The type is specified in the message.
							//
							dragMessage.AddString("be:types", B_FILE_MIME_TYPE);
							dragMessage.AddString("be:filetypes",
								enclosure->content_type);
							dragMessage.AddString("be:clip_name", enclosure->name);
							break;
	
						case TYPE_URL:
							//
							// URL.  The user can drag it into the tracker to 
							// create a bookmark file.
							//
							dragMessage.AddString("be:types", B_FILE_MIME_TYPE);
							dragMessage.AddString("be:filetypes",
							  "application/x-vnd.Be-bookmark");
							dragMessage.AddString("be:filetypes", "text/plain");
							dragMessage.AddString("be:clip_name", "Bookmark");
							
							dragMessage.AddString("be:url", enclosure->name);
							break;
	
						case TYPE_MAILTO:
							//
							// Mailto address.  The user can drag it into the
							// tracker to create a people file.
							//
							dragMessage.AddString("be:types", B_FILE_MIME_TYPE);
							dragMessage.AddString("be:filetypes", 
							  "application/x-person");
							dragMessage.AddString("be:filetypes", "text/plain");
							dragMessage.AddString("be:clip_name", "Person");
	
							dragMessage.AddString("be:email", enclosure->name);
							break;

						default:
							//	
							// Otherwise it doesn't have a type that I know how
							// to save.  It won't have any types and if any
							// program wants to accept it, more power to them.
							// (tracker won't.)
							//	
							dragMessage.AddString("be:clip_name", "Hyperlink");
					};

					BMessage data;
					data.AddPointer("enclosure", enclosure);
					dragMessage.AddMessage("be:originator-data", &data);
		
					BRegion selectRegion;
					GetTextRegion(start, finish, &selectRegion);
					DragMessage(&dragMessage, selectRegion.Frame(), this);
				}
				else
				{
					//
					//	User Single clicked on the attachment.  Open it.
					//
					Open(enclosure);					
				}
			}
			return;
		}
	}
	BTextView::MouseDown(where);
}


void TTextView::MouseMoved(BPoint where, uint32 code, const BMessage *msg)
{
	int32 start = OffsetAt(where);

	for (int32 loop = fEnclosures->CountItems(); loop-- > 0;)
	{
		hyper_text *enclosure = (hyper_text *)fEnclosures->ItemAt(loop);
		if ((start >= enclosure->text_start) && (start < enclosure->text_end))
		{
			if (!fCursor)
				SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
			fCursor = true;
			return;
		}
	}
	
	if (fCursor)
	{
		SetViewCursor(B_CURSOR_I_BEAM);
		fCursor = false;
	}
	
	BTextView::MouseMoved(where, code, msg);
}


void TTextView::ClearList()
{
	BEntry			entry;
	hyper_text		*enclosure;

	while ((enclosure = (hyper_text *)fEnclosures->FirstItem()) != NULL)
	{
		fEnclosures->RemoveItem(enclosure);
		if (enclosure->name)
			free(enclosure->name);
		if (enclosure->content_type)
			free(enclosure->content_type);
		if (enclosure->encoding)
			free(enclosure->encoding);
		if ((enclosure->have_ref) && (!enclosure->saved))
		{
			entry.SetTo(&enclosure->ref);
			entry.Remove();
		}
//		if (enclosure->delete_component)
//			delete enclosure->component;

		watch_node(&enclosure->node, B_STOP_WATCHING, this);
		free(enclosure);
	}
}


void TTextView::LoadMessage(BFile *file, bool quoteIt, bool close, const char *text)
{
	Window()->Unlock();
	StopLoad();		
	Window()->Lock();

	delete fMail;  fMail = NULL;
	fFile = file;

	ClearList();
	MakeSelectable(true);
	MakeEditable(false);
	if (text)
		Insert(text, strlen(text));

	attr_info attrInfo;
	TTextView::Reader *reader = new TTextView::Reader(fHeader, fRaw, quoteIt, fIncoming,
				close, file->GetAttrInfo(B_MAIL_ATTR_MIME, &attrInfo) == B_OK,
				this, file, fEnclosures, fStopSem);
	if (close)
		delete fFile;

	resume_thread(fThread = spawn_thread(Reader::Run, "reader", B_NORMAL_PRIORITY, reader));
}


void TTextView::Open(hyper_text *enclosure)
{
	switch (enclosure->type)
	{
		case TYPE_URL:
		{
			const struct {const char *urlType, *handler; } handlerTable[] =
			{
				{"http",	B_URL_HTTP},
				{"https",	B_URL_HTTPS},
				{"ftp",		B_URL_FTP},
				{"gopher",	B_URL_GOPHER},
				{"mailto",	B_URL_MAILTO},
				{"news",	B_URL_NEWS},
				{"nntp",	B_URL_NNTP},
				{"telnet",	B_URL_TELNET},
				{"rlogin",	B_URL_RLOGIN},
				{"tn3270",	B_URL_TN3270},
				{"wais",	B_URL_WAIS},
				{"file",	B_URL_FILE},
				{NULL,		NULL}
			};
			const char *handlerToLaunch = NULL;

			const char *colonPos = strchr(enclosure->name, ':');
			if (colonPos)
			{
				int urlTypeLength = colonPos - enclosure->name;
				
				for (int32 index = 0; handlerTable[index].urlType; index++)
				{
					if (strncasecmp(enclosure->name, handlerTable[index].urlType, urlTypeLength) == 0)
					{
						handlerToLaunch = handlerTable[index].handler;
						break;
					}
				}
			}
			if (handlerToLaunch)
			{
				entry_ref appRef;
				if (be_roster->FindApp(handlerToLaunch, &appRef) != B_OK)
					handlerToLaunch = NULL;
			}
			if (!handlerToLaunch)
				handlerToLaunch = "application/x-vnd.Be-Bookmark";

			status_t result = be_roster->Launch(handlerToLaunch, 1, &enclosure->name);
			if ((result != B_NO_ERROR) && (result != B_ALREADY_RUNNING))
			{
				beep();
				(new BAlert("", "There is no installed handler for URL links.",
					"Sorry"))->Go();
			}
			break;
		}

		case TYPE_MAILTO:
			be_roster->Launch(B_MAIL_TYPE, 1, &enclosure->name);
			break;

		case TYPE_ENCLOSURE:
		case TYPE_BE_ENCLOSURE:
			if (!enclosure->have_ref)
			{
				BPath path;
				if (find_directory(B_COMMON_TEMP_DIRECTORY, &path) == B_NO_ERROR)
				{
					BDirectory dir(path.Path());
					if (dir.InitCheck() == B_NO_ERROR)
					{
						char name[B_FILE_NAME_LENGTH];
						char baseName[B_FILE_NAME_LENGTH];
						strcpy(baseName, enclosure->name ? enclosure->name : "enclosure");
						strcpy(name, baseName);
						for (int32 index = 0; dir.Contains(name); index++)
							sprintf(name, "%s_%ld", baseName, index);

						BEntry entry(path.Path());
						entry_ref ref;
						entry.GetRef(&ref);

						BMessage save(M_SAVE);
						save.AddRef("directory", &ref);
						save.AddString("name", name);
						save.AddPointer("enclosure", enclosure);
						if (Save(&save) != B_NO_ERROR)
							break;
						enclosure->saved = false;
					}
				}
			}
			BMessenger tracker("application/x-vnd.Be-TRAK");
			if (tracker.IsValid())
			{
				BMessage openMsg(B_REFS_RECEIVED);
				openMsg.AddRef("refs", &enclosure->ref);
				tracker.SendMessage(&openMsg);
			}
			break;
	}
}


status_t TTextView::Save(BMessage *msg, bool makeNewFile)
{
	const char		*name;
	entry_ref		ref;
	BFile			file;
	BPath			path;
	hyper_text		*enclosure;
	status_t		result = B_NO_ERROR;
	char 			entry_name[B_FILE_NAME_LENGTH];

	msg->FindString("name", &name);
	msg->FindRef("directory", &ref);
	msg->FindPointer("enclosure", (void **)&enclosure);

	BDirectory dir;
	dir.SetTo(&ref);
	result = dir.InitCheck();

	if (result == B_OK)
	{
		if (makeNewFile)
		{
			//
			// Search for the file and delete it if it already exists.
			// (It may not, that's ok.)
			//
			BEntry entry;
			if (dir.FindEntry(name, &entry) == B_NO_ERROR)
				entry.Remove();
			
			if ((enclosure->have_ref) && (!enclosure->saved))
			{
				entry.SetTo(&enclosure->ref);
	
				//
				// Added true arg and entry_name so MoveTo clobbers as 
				// before. This may not be the correct behaviour, but
				// it's the preserved behaviour.
				//
				entry.GetName(entry_name);
				result = entry.MoveTo(&dir, entry_name, true);
				if (result == B_NO_ERROR) {
					entry.Rename(name);
					entry.GetRef(&enclosure->ref);
					entry.GetNodeRef(&enclosure->node);
					enclosure->saved = true;
					return result;
				}
			}
					
			if (result == B_NO_ERROR)
			{
				result = dir.CreateFile(name, &file);
				if (result == B_NO_ERROR && enclosure->content_type)
				{
					BNodeInfo info(&file);
					info.SetType(enclosure->content_type);
				}
			}
		}
		else
		{
			//
			// 	This file was dragged into the tracker or desktop.  The file 
			//	already exists.
			//
			result = file.SetTo(&dir, name, B_WRITE_ONLY);
		}
	}
		
	if (result == B_NO_ERROR)
	{
		//
		// Write the data
		//
		enclosure->component->GetDecodedData(&file);
		
		BEntry entry;
		dir.FindEntry(name, &entry);
		entry.GetRef(&enclosure->ref);
		enclosure->have_ref = true;
		enclosure->saved = true;
		entry.GetPath(&path);
		update_mime_info(path.Path(), false, true,
			!cistrcmp("application/octet-stream", enclosure->content_type));
		entry.GetNodeRef(&enclosure->node);
		watch_node(&enclosure->node, B_WATCH_NAME, this);
	}	

	if (result != B_NO_ERROR)
	{
		beep();
		(new BAlert("", "An error occurred trying to save the enclosure.",
			"Sorry"))->Go();
	}

	return result;
}


void TTextView::StopLoad()
{
//	thread_id thread = fThread;
	thread_info	info;
	if (fThread != 0 && get_thread_info(fThread, &info) == B_NO_ERROR)
	{
		acquire_sem(fStopSem);
		int32 result;
		wait_for_thread(fThread, &result);
		fThread = 0;
		release_sem(fStopSem);
	}
}


void TTextView::AddAsContent(MailMessage *mail, bool wrap)
{
	if (mail == NULL)
		return;

	const char	*text = Text();
	int32		textLen = TextLength();

	PlainTextBodyComponent *body = mail->Body();
	if (body == NULL)
	{
		if (mail->SetBody(body = new PlainTextBodyComponent()) < B_OK)
			return;
		body->SetEncoding(quoted_printable,gMailEncoding);
	}
	if (!wrap)
		body->AppendText(text); //, textLen, mail_encoding);
	else {
		BWindow	*window = Window();
		BRect	saveTextRect = TextRect();
		
		// do this before we start messing with the fonts
		// the user will never know...
		window->DisableUpdates();
		Hide();
		BScrollBar *vScroller = ScrollBar(B_VERTICAL);
		BScrollBar *hScroller = ScrollBar(B_HORIZONTAL);
		if (vScroller != NULL)
			vScroller->SetTarget((BView *)NULL);
		if (hScroller != NULL)
			hScroller->SetTarget((BView *)NULL);

		// temporarily set the font to be_fixed_font 				
		bool wasStylable = IsStylable();
		SetStylable(true);
		SetFontAndColor(0, textLen, be_fixed_font);
		
		if (gMailEncoding == B_JIS_CONVERSION) {
			// this is truly evil...  I'm ashamed of myself (Hiroshi)
			int32	lastMarker = 0;
			bool 	inKanji = false;
			BFont	kanjiFont(be_fixed_font);
			kanjiFont.SetSize(kanjiFont.Size() * 2);
		
			for (int32 i = 0; i < textLen; i++) {
				if (ByteAt(i) > 0x7F) {
					if (!inKanji) {
						SetFontAndColor(lastMarker, i, be_fixed_font, B_FONT_SIZE);
						lastMarker = i;
						inKanji = true;
					}
				} else {
					if (inKanji) {
						SetFontAndColor(lastMarker, i, &kanjiFont, B_FONT_SIZE);
						lastMarker = i;
						inKanji = false;
					}
				}	
			}
		
			if (inKanji)
				SetFontAndColor(lastMarker, textLen, &kanjiFont, B_FONT_SIZE);
		}

		// calculate a text rect that is 72 columns wide
		BRect newTextRect = saveTextRect;
		newTextRect.right = newTextRect.left
			+ (be_fixed_font->StringWidth("m") * 72);	
		SetTextRect(newTextRect);

		// hard-wrap, based on TextView's soft-wrapping
		int32	numLines = CountLines();
		char	*content = (char *)malloc(textLen + numLines);	// most we'll ever need
		if (content != NULL) {
			int32 contentLen = 0;

			for (int32 i = 0; i < numLines; i++) {
				int32 startOffset = OffsetAt(i);
				int32 endOffset = OffsetAt(i + 1);
				int32 lineLen = endOffset - startOffset;

				memcpy(content + contentLen, text + startOffset, lineLen);
				contentLen += lineLen;

				// add a newline to every line except for the ones
				// that already end in newlines, and the last line 
				if ((text[endOffset - 1] != '\n') && (i < (numLines - 1)))
					content[contentLen++] = '\n';
			}
			content[contentLen] = '\0';

			body->AppendText(content); //, contentLen, mail_encoding);
			free(content);
		}

		// reset the text rect and font			
		SetTextRect(saveTextRect);	
		SetFontAndColor(0, textLen, &fFont);
		SetStylable(wasStylable);

		// should be OK to hook these back up now
		if (vScroller != NULL)
			vScroller->SetTarget(this);
		if (hScroller != NULL)
			hScroller->SetTarget(this);				
		Show();
		window->EnableUpdates();
	}
}


//--------------------------------------------------------------------
//	#pragma mark -


TTextView::Reader::Reader(bool header, bool raw, bool quote, bool incoming, bool close,
				bool mime, TTextView *view, BFile *file, BList *list, sem_id sem)
	:	fHeader(header),
		fRaw(raw),
		fQuote(quote),
		fIncoming(incoming),
		fClose(close),
		fMime(mime),
		fView(view),
		fFile(*file),
		fEnclosures(list),
		fStopSem(sem)
{
}


bool TTextView::Reader::ParseMail(MailContainer *container,PlainTextBodyComponent *ignore)
{
	//MailComponent *c = new MailComponent();
	int32 count = 0;
	for (int32 i = 0;i < container->CountComponents();i++)
	{
		MailComponent *component;
		/*if (container->ManualGetComponent(c,i) == B_OK)
			component = c;
		else*/ if ((component = container->GetComponent(i)) == NULL)
			continue;

		count++;
		if (component == ignore)
			continue;

		if (component->ComponentType() == MC_MULTIPART_CONTAINER)
		{
			MIMEMultipartContainer *c = (MIMEMultipartContainer *)container->GetComponent(i);
			if (!ParseMail(c,ignore))
				count--;
		}
		else if (fIncoming)
		{
			hyper_text *enclosure = (hyper_text *)malloc(sizeof(hyper_text));
			memset(enclosure, 0, sizeof(hyper_text));

			enclosure->type = TYPE_ENCLOSURE;
			enclosure->component = component;
//			enclosure->delete_component = component == c;
//			if (component == c)
//				c = new MailComponent();

			BString name;
			char fileName[B_FILE_NAME_LENGTH];
			strcpy(fileName,"untitled");
			if (MailAttachment *attachment = dynamic_cast<MailAttachment *>(component))
				attachment->FileName(fileName);

//			if (!get_parameter(component->HeaderField("Content-Type"),"name=",&name)
//				&& !get_parameter(component->HeaderField("Content-Disposition"),"name=",&name))
//				name << "untitled";

			BPath path(fileName);
			enclosure->name = strdup(path.Leaf());
			
			BMimeType type;
			component->MIMEType(&type);
			enclosure->content_type = strdup(type.Type());

			char typeDescription[B_MIME_TYPE_LENGTH];
			if (type.GetShortDescription(typeDescription) != B_OK)
				strcpy(typeDescription, type.Type());
			
			name = "\n<Enclosure: ";
			name << enclosure->name << " (Type: " << typeDescription << ")>\n";
//					strcpy(enclosure->content_type, contentType);
			fView->GetSelection(&enclosure->text_start, &enclosure->text_end);
			enclosure->text_start++;
			enclosure->text_end += strlen(name.String()) - 1;
//					enclosure->file_offset = offset - base;
//					enclosure->file_length = len;
			Insert(name.String(), name.Length(), true);
			fEnclosures->AddItem(enclosure);
		}
//			default:
//			{
//				PlainTextBodyComponent *body = dynamic_cast<PlainTextBodyComponent *>(container->GetComponent(i));
//				const char *text;
//				if (body && (text = body->Text()) != NULL)
//					Insert(text, strlen(text), false);
//			}
	}
//	delete c;
	return count > 0;
}


bool TTextView::Reader::Process(const char *data, int32 data_len, bool isHeader)
{
	const char *urlPrefixes[] = {
		"http://",
		"ftp://",
		"shttp://",
		"https://",
		"finger://",
		"telnet://",
		"gopher://",
		"news://",
		"nntp://",
		0
	};
	bool	bracket = false;
	char	line[522];
	int32	count = 0;
	int32	index;
	int32	type;

	for (int32 loop = 0; loop < data_len; loop++)
	{
		if ((fQuote) && ((!loop) || ((loop) && (data[loop - 1] == '\n'))))
		{
			strcpy(&line[count], QUOTE);
			count += strlen(QUOTE);
		}
		if (!fRaw && loop && data[loop - 1] == '\n' && data[loop] == '.')
			continue;

		if (!fRaw && fIncoming && (loop < data_len - 7))
		{
			type = 0;

			//
			//	Search for URL prefix
			//
			for (const char **i = urlPrefixes; *i != 0; ++i) {
				if (!cistrncmp(&data[loop], *i, strlen(*i))) {
					type = TYPE_URL;
					break;	
				}
			}

			//
			//	Not a URL? check for mailto.
			//
			if (type == 0 && !cistrncmp(&data[loop], "mailto:", strlen("mailto:")))
					type = TYPE_MAILTO;

			if (type)
			{
				if (type == TYPE_URL)
					index = strcspn(data+loop, " <>\"\r\n");
				else
					index = strcspn(data+loop, " \t>)\"\\,\r\n");
				
				/*while ((data[loop + index] != ' ') &&
					   (data[loop + index] != '\t') &&
					   (data[loop + index] != '>') &&
					   (data[loop + index] != ')') &&
					   (data[loop + index] != '"') &&
					   (data[loop + index] != '\'') &&
					   (data[loop + index] != ',') &&
					   (data[loop + index] != '\r')) {
					index++;
				}*/

				if ((loop) && (data[loop - 1] == '<')
						&& (data[loop + index] == '>')) {
					if (!Insert(line, count - 1, false, isHeader))
						return false;
					bracket = true;
				}
				else if (!Insert(line, count, false, isHeader))
					return false;
				count = 0;

				hyper_text *enclosure = (hyper_text *)malloc(sizeof(hyper_text));
				memset(enclosure, 0, sizeof(hyper_text));
				fView->GetSelection(&enclosure->text_start,
									&enclosure->text_end);
				enclosure->type = type;
				enclosure->name = (char *)malloc(index + 1);
				memcpy(enclosure->name, &data[loop], index);
				enclosure->name[index] = 0;
				if (bracket) {
					Insert(&data[loop - 1], index + 2, true, isHeader);
					enclosure->text_end += index + 2;
					bracket = false;
					loop += index;
				} else {
					Insert(&data[loop], index, true, isHeader);
					enclosure->text_end += index;
					loop += index - 1;
				}
				fEnclosures->AddItem(enclosure);
				continue;
			}
		}
		if (!fRaw && fMime && data[loop] == '=')
		{
			if ((loop) && (loop < data_len - 1) && (data[loop + 1] == '\r'))
				loop += 2;
			else
				line[count++] = data[loop];
		}
		else if (data[loop] != '\r')
			line[count++] = data[loop];

		if ((count > 511) || ((count) && (loop == data_len - 1)))
		{
			if (!Insert(line, count, false, isHeader))
				return false;
			count = 0;
		}
	}
	return true;
}


bool TTextView::Reader::Insert(const char *line, int32 count, bool isHyperLink, bool isHeader)
{
	if (!count)
		return true;

	const rgb_color hyper_color = {0, 0, 255, 0};
	const rgb_color header_color = {72,72,72, 0};
	const rgb_color normal_color = { 0, 0, 0, 0};

	BFont font(fView->fFont);
	text_run_array style;
	style.count = 1;
	style.runs[0].offset = 0;	
	if (isHeader)
	{
		style.runs[0].color = isHyperLink ? hyper_color : header_color;
		font.SetSize(font.Size() * 0.9);
	}
	else
		style.runs[0].color = isHyperLink ? hyper_color : normal_color;
	style.runs[0].font = font;

	if (!Lock())
		return false;
	fView->Insert(line, count, &style);
	Unlock();

	return true;
}


status_t TTextView::Reader::Run(void *_this)
{
	Reader *reader = (Reader *)_this;
	char		*msg;
	int32		len;
	off_t		size;

	reader->fFile.GetSize(&size);
	if ((msg = (char *)malloc(size)) == NULL)
		goto done;
	reader->fFile.Seek(0, 0);
	size = reader->fFile.Read(msg, size);
	len = header_len(&reader->fFile);

	// show the header?
	if (reader->fHeader && len && !reader->Process(msg, len, true))
		goto done;

	if (reader->fRaw)
	{
		if (!reader->Process((const char *)msg + len, size - len))
			goto done;
	}
	else if (!reader->fMime)
	{
		// convert to user's preferred encoding if charset not specified in MIME
		int32	convState = 0;
		int32	src_len = size - len;
		int32	dst_len = 4 * src_len;
		char	*utf8 = (char *)malloc(dst_len);

		convert_to_utf8(gMailEncoding, msg + len, &src_len, utf8, &dst_len,
			&convState);

		bool result = reader->Process((const char *)utf8, dst_len);
		free(utf8);

		if (!result)
			goto done;
	}
	else
	{
		reader->fFile.Seek(0, 0);
		MailMessage *mail = new MailMessage(&reader->fFile);
		
		// at first, insert the mail body
		if (mail->BodyText())
			reader->Process(mail->BodyText(), strlen(mail->BodyText()));

		if (!reader->ParseMail(mail,mail->Body()))
		{
			delete mail;
			goto done;
		}
		reader->fView->fMail = mail;
	}
//	else if (!parse_header(msg, msg, size, NULL, info, NULL))
//		goto done;

	if (reader->Lock())
	{
		reader->fView->Select(0, 0);
		reader->fView->MakeSelectable(true);
		if (!reader->fIncoming)
			reader->fView->MakeEditable(true);

		reader->Unlock();
	}

done:
	delete reader;
	if (msg)
		free(msg);

	return B_NO_ERROR;
}


status_t TTextView::Reader::Unlock()
{
	fView->Window()->Unlock();
	return release_sem(fStopSem);
}


bool TTextView::Reader::Lock()
{
	BWindow *window = fView->Window();

	if (!window->Lock())
		return false;
	if (acquire_sem_etc(fStopSem, 1, B_TIMEOUT, 0) != B_NO_ERROR)
	{
		window->Unlock();
		return false;
	}
	return true;
}


//====================================================================
//	#pragma mark -


TSavePanel::TSavePanel(hyper_text *enclosure, TTextView *view)
		   :BFilePanel(B_SAVE_PANEL)
{
	fEnclosure = enclosure;
	fView = view;
	if (enclosure->name)
		SetSaveText(enclosure->name);
}


void TSavePanel::SendMessage(const BMessenger * /* messenger */, BMessage *msg)
{
	const char	*name = NULL;
	BMessage	save(M_SAVE);
	entry_ref	ref;

	if ((!msg->FindRef("directory", &ref)) && (!msg->FindString("name", &name))) {
		save.AddPointer("enclosure", fEnclosure);
		save.AddString("name", name);
		save.AddRef("directory", &ref);
		fView->Window()->PostMessage(&save, fView);
	}
}


void TSavePanel::SetEnclosure(hyper_text *enclosure)
{
	fEnclosure = enclosure;
	if (enclosure->name)
		SetSaveText(enclosure->name);
	else
		SetSaveText("");
	if (!IsShowing())
		Show();
	Window()->Activate();
}


//--------------------------------------------------------------------
//	#pragma mark -


void TTextView::InsertText(const char *text, int32 length, int32 offset,
	const text_run_array *runs)
{
	ContentChanged();
	
	if (fSpellCheck && IsEditable())
	{
		BTextView::InsertText(text, length, offset, NULL);
		rgb_color color;
		GetFontAndColor(offset-1, NULL, &color);
		const char *text = Text();
		
		if ((length > 1) || isalpha(text[offset+1]) || ((!isalpha(text[offset]))
				&& (text[offset]!='\'')) || (color.red != 0)) {
			int32 start, end;
			FindSpellBoundry(length, offset, &start, &end);
			//printf("Offset %ld, start %ld, end %ld\n", offset, start, end);
			CheckSpelling(start, end);
		}
	}
	else
		BTextView::InsertText(text, length, offset, runs);
}

void TTextView::DeleteText(int32 start, int32 finish)
{
	ContentChanged();
	BTextView::DeleteText(start, finish);
	if (fSpellCheck && IsEditable())
	{
		int32 s, e;
		FindSpellBoundry(1, start, &s, &e);
		CheckSpelling(s, e);
	}
}

void TTextView::ContentChanged(void)
{
	BLooper *looper;
	if ((looper=Looper()) != NULL)
	{
		BMessage msg(FIELD_CHANGED);
		msg.AddInt32("bitmask", FIELD_BODY);
		msg.AddPointer("source", this);
		looper->PostMessage(&msg);
	}
}

void TTextView::CheckSpelling(int32 start, int32 end, int32 flags)
{
	//printf("Check spelling\n");
	const char 	*text = Text();
	const char 	*next, *endPtr, *word;
	int32 		wordLength, wordOffset;
	int32		nextHighlight = start;
	
	int32		key = -1; // dummy value -- not used
	BString 	testWord;
	rgb_color 	plainColor = { 0, 0, 0, 255 };
	rgb_color	flagColor = { 255, 0, 0, 255 };
	bool		isCap = false;
	bool		isAlpha;
	bool		isApost;
	
	for (next=text+start, endPtr=text+end, wordLength=0, word=NULL; next<=endPtr;
			next++) {
		//printf("next=%c\n", *next);
		// Alpha signifies the start of a word
		isAlpha = isalpha(*next);
		isApost = (*next=='\'');
		if (!word && isAlpha) {
			//printf("Found word start\n");
			word = next;
			wordLength++;
			isCap = isupper(*word);
		}
		// Word continues check
		else if (word && (isAlpha || isApost) && !(isApost && !isalpha(next[1]))
				&& !(isCap && isApost && (next[1]=='s'))) {
			wordLength++;
			//printf("Word continues...\n");
		}
		// End of word reached
		else if (word) {
			//printf("Word End\n");
			// Don't check single characters
			if (wordLength > 1) {
				bool isUpper = true;
				
				// Look for all uppercase
				for (int32 i=0; i<wordLength; i++) {
					if (word[i] == '\'')
						break;
					if (islower(word[i])) {
						isUpper = false;
						break;
					}
				}
				
				// Don't check all uppercase words
				if (!isUpper) {
					bool foundMatch = false;
					wordOffset = word-text;
					testWord.SetTo(word, wordLength);
					
					testWord = testWord.ToLower();
					//printf("Testing: %s:\n", testWord.String());
					
					if (gDictCount)
						key = gExactWords[0]->GetKey(testWord.String());
					
					// Search all dictionaries
					for (int32 i=0; i<gDictCount; i++) {
						// printf("Looking for %s in dict %ld\n", testWord.String(),
						// i); Is it in the words index?
						if (gExactWords[i]->Lookup(key) >= 0) {
							foundMatch = true;
							break;
						}
					}
					
					if (!foundMatch) {
						if (flags & S_CLEAR_ERRORS)
							SetFontAndColor(nextHighlight, wordOffset, NULL,
								B_FONT_ALL, &plainColor);
						if (flags & S_SHOW_ERRORS)
							SetFontAndColor(wordOffset, wordOffset+wordLength, NULL,
								B_FONT_ALL, &flagColor);
					}
					else if (flags & S_CLEAR_ERRORS)
						SetFontAndColor(nextHighlight, wordOffset+wordLength, NULL,
							B_FONT_ALL, &plainColor);
					
					nextHighlight = wordOffset+wordLength;
				}
			}
			// Reset state to looking for word
			word = NULL;
			wordLength = 0;
		}
	}
	if ((nextHighlight <= end)&&(flags & S_CLEAR_ERRORS)
			&& (nextHighlight<TextLength()))
		SetFontAndColor(nextHighlight, end, NULL, B_FONT_ALL, &plainColor);
}

void TTextView::FindSpellBoundry(int32 length, int32 offset, int32 *s, int32 *e)
{
	int32 start, end, textLength;
	const char *text = Text();
	textLength = TextLength();
	for (start = offset-1; start >= 0 && (isalpha(text[start])||(text[start]=='\'')); start--) {} start++;
	for (end = offset+length; end < textLength && (isalpha(text[end])
		|| (text[end]=='\'')); end++) {}
	*s = start;
	*e = end;
}

void TTextView::EnableSpellCheck(bool enable)
{
	if (fSpellCheck != enable) {
		fSpellCheck = enable;
		int32 textLength = TextLength();
		if (fSpellCheck) {
			SetStylable(true);
			CheckSpelling(0, textLength);
		} else {
			rgb_color plainColor = { 0, 0, 0, 255 };
			SetFontAndColor(0, textLength, NULL, B_FONT_ALL, &plainColor);
			SetStylable(false);
		}
	}
}
