/* ConfigView - the configuration view for the Folder filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <TextControl.h>
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


BView* instantiate_config_panel(BMessage *settings, BMessage *meta_data)
{
	ConfigView *view = new ConfigView();
	view->SetTo(settings,meta_data);

	return view;
}
