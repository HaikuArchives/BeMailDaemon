/* ConfigView - the configuration view for the Folder filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <TextControl.h>
#include <Button.h>
#include <String.h>
#include <Message.h>

#include <MailAddon.h>

ConfigView::ConfigView()
	:	BView(BRect(0,0,20,20),"folder_config",B_FOLLOW_LEFT | B_FOLLOW_TOP,0)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// determine font height
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float itemHeight = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 13;

	BRect rect(5,4,250,25);
	rect.bottom = rect.top - 2 + itemHeight;
	BTextControl *control = new BTextControl(rect,"destination_path","Destination Folder:",NULL,NULL);
	control->SetDivider(control->StringWidth(control->Label()) + 6);
	AddChild(control);

	ResizeToPreferred();
}		


void ConfigView::SetTo(BMessage *archive, BMessage *meta_data)
{
	meta = meta_data;
	BString path = meta_data->FindString("path");
	
	if (path == "")
		path = "/boot/home/mail/in";
	
	if (BTextControl *control = (BTextControl *)FindView("destination_path"))
		control->SetText(path.String());
}


status_t ConfigView::Archive(BMessage *into,bool) const
{
	if (BTextControl *control = (BTextControl *)FindView("destination_path"))
	{
		if (meta->ReplaceString("path",control->Text()) != B_OK)
			meta->AddString("path",control->Text());
	}

	return B_OK;
}

	
void ConfigView::GetPreferredSize(float *width, float *height)
{
	*width = 258;
	*height = ChildAt(0)->Bounds().Height() + 8;
}


FileControl::FileControl(BRect rect,const char *label,const char *pathOfFile)
	:	BView(rect,"file_config",B_FOLLOW_LEFT | B_FOLLOW_TOP,0)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// determine font height
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float itemHeight = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 13;

	rect.left = 5;  rect.right -= 62;
	rect.bottom = rect.top - 2 + itemHeight;
	rect.OffsetBy(0,4);
	fText = new BTextControl(rect,"file_path",label,pathOfFile,NULL);
	fText->SetDivider(fText->StringWidth(label) + 6);
	AddChild(fText);

	rect.left = rect.right + 6;
	rect.right += 62;
	rect.OffsetBy(0,-3);
	fButton = new BButton(rect,"select_file","Select...",NULL);
	AddChild(fButton);

	ResizeToPreferred();
}


void FileControl::SetText(const char *pathOfFile)
{
	fText->SetText(pathOfFile);
}


const char *FileControl::Text() const
{
	return fText->Text();
}


void FileControl::GetPreferredSize(float *width, float *height)
{
	*width = fButton->Frame().right + 5;
	*height = fText->Bounds().Height() + 8;
}


FileConfigView::FileConfigView(const char *label,const char *name,bool useMeta,const char *defaultPath)
		: FileControl(BRect(0,0,255,10),label,defaultPath),
		fName(name),
		fUseMeta(useMeta)
{
}

#include <stdio.h>
void FileConfigView::SetTo(BMessage *archive, BMessage *meta)
{
	fMeta = meta;
printf("meta: %p, archive: %p, name: %s, use meta: %s\n",meta,archive,fName,fUseMeta ? "yes" : "no");
	BString path = (fUseMeta ? meta : archive)->FindString(fName);
puts(path.String());
	if (path != "")
		SetText(path.String());
}


status_t FileConfigView::Archive(BMessage *into, bool deep) const
{
	const char *path = Text();
	BMessage *archive = fUseMeta ? fMeta : into;

	if (archive->ReplaceString(fName,path) != B_OK)
		archive->AddString(fName,path);

	return B_OK;
}


BView* instantiate_config_panel(BMessage *settings, BMessage *meta_data)
{
//	ConfigView *view = new ConfigView();
	FileConfigView *view = new FileConfigView("Destination Folder:","path",true,"/boot/home/mail/in");
	view->SetTo(settings,meta_data);

	return view;
}
