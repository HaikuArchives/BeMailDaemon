/* ConfigView - the configuration view for the Folder filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <TextControl.h>
#include <String.h>
#include <Message.h>


ConfigView::ConfigView()
	:	BView(BRect(0,0,20,20),"producer_config",B_FOLLOW_LEFT | B_FOLLOW_TOP,0)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// determine font height
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float itemHeight = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 13;

	BRect rect(5,4,250,25);
	rect.bottom = rect.top - 2 + itemHeight;
	BTextControl *control = new BTextControl(rect,"source_path","Source Folder:",NULL,NULL);
	control->SetDivider(control->StringWidth(control->Label()) + 6);
	AddChild(control);

	ResizeToPreferred();
}		


void ConfigView::SetTo(BMessage *archive)
{
	BString path = archive->FindString("source_path");
	if (path == "")
		path << "/boot/home/mail/out";

	if (BTextControl *control = (BTextControl *)FindView("source_path"))
		control->SetText(path.String());
}


status_t ConfigView::Archive(BMessage *into,bool) const
{
	if (BTextControl *control = (BTextControl *)FindView("source_path"))
	{
		if (into->ReplaceString("source_path",control->Text()) != B_OK)
			into->AddString("source_path",control->Text());
	}

	return B_OK;
}

	
void ConfigView::GetPreferredSize(float *width, float *height)
{
	*width = 258;
	*height = ChildAt(0)->Bounds().Height() + 8;
}

