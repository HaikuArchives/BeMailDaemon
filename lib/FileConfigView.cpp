/* FileConfigView - a file configuration view for filters
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <TextControl.h>
#include <Button.h>
#include <String.h>
#include <Message.h>
#include <Path.h>

#include <stdio.h>

class _EXPORT FileControl;

namespace Mail {
	class _EXPORT FileConfigView;
}

using Mail::FileConfigView;

#include "FileConfigView.h"

const uint32 kMsgSelectButton = 'fsel';


FileControl::FileControl(BRect rect,const char *name,const char *label,const char *pathOfFile,uint32 flavors)
		:	BView(rect,name,B_FOLLOW_LEFT | B_FOLLOW_TOP,0)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// determine font height
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float itemHeight = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 13;
	float labelWidth = StringWidth("Select" B_UTF8_ELLIPSIS) + 20;
	rect = Bounds();
	rect.right -= labelWidth;
	rect.top = 4;	rect.bottom = itemHeight + 2;
	fText = new BTextControl(rect,"file_path",label,pathOfFile,NULL);
	if (label)
		fText->SetDivider(fText->StringWidth(label) + 6);
	AddChild(fText);

	rect.left = rect.right + 6;
	rect.right += labelWidth;
	rect.OffsetBy(0,-3);
	fButton = new BButton(rect,"select_file","Select" B_UTF8_ELLIPSIS,new BMessage(kMsgSelectButton));
	AddChild(fButton);

	fPanel = new BFilePanel(B_OPEN_PANEL,NULL,NULL,flavors,false);

	ResizeToPreferred();
}


FileControl::~FileControl()
{
	delete fPanel;
}


void FileControl::AttachedToWindow()
{
	fButton->SetTarget(this);

	BMessenger messenger(this);
	if (messenger.IsValid())
		fPanel->SetTarget(messenger);
}


void FileControl::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case kMsgSelectButton:
		{
			fPanel->Hide();
			//fPanel->Window()->SetTitle(title);

			BPath path(fText->Text());
			if (path.InitCheck() >= B_OK)
				if (path.GetParent(&path) >= B_OK)
					fPanel->SetPanelDirectory(path.Path());

			fPanel->Show();
			break;
		}
		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (msg->FindRef("refs",&ref) >= B_OK)
			{
				BEntry entry(&ref);
				if (entry.InitCheck() >= B_OK)
				{
					BPath path;
					entry.GetPath(&path);

					fText->SetText(path.Path());
				}
			}
			break;
		}
		default:
			BView::MessageReceived(msg);
			break;
	}
}


void FileControl::SetText(const char *pathOfFile)
{
	fText->SetText(pathOfFile);
}


const char *FileControl::Text() const
{
	return fText->Text();
}


void FileControl::SetEnabled(bool enabled)
{
	fText->SetEnabled(enabled);
	fButton->SetEnabled(enabled);
}


void FileControl::GetPreferredSize(float *width, float *height)
{
	*width = fButton->Frame().right + 5;
	*height = fText->Bounds().Height() + 8;
}


//--------------------------------------------------------------------------
//	#pragma mark -


FileConfigView::FileConfigView(const char *label,const char *name,bool useMeta,const char *defaultPath,uint32 flavors)
		:	FileControl(BRect(5,0,255,10),name,label,defaultPath,flavors),
		fUseMeta(useMeta),
		fName(name)
{
}


void FileConfigView::SetTo(BMessage *archive, BMessage *meta)
{
	fMeta = meta;
	BString path = (fUseMeta ? meta : archive)->FindString(fName);

	if (path != "")
		SetText(path.String());
}


status_t FileConfigView::Archive(BMessage *into, bool /*deep*/) const
{
	const char *path = Text();
	BMessage *archive = fUseMeta ? fMeta : into;

	if (archive->ReplaceString(fName,path) != B_OK)
		archive->AddString(fName,path);

	return B_OK;
}

