/* ConfigView - the configuration view for the Notifier filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <MenuField.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <String.h>
#include <Message.h>


ConfigView::ConfigView()
	:	BView(BRect(0,0,20,20),"notifier_config",B_FOLLOW_LEFT | B_FOLLOW_TOP,0)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// determine font height
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float itemHeight = (int32)(fontHeight.ascent + fontHeight.descent + fontHeight.leading) + 13;

	BRect rect(5,4,250,25);
	rect.bottom = rect.top - 2 + itemHeight;

	BPopUpMenu *menu = new BPopUpMenu("<select method>");
	char *strings[] = {"Beep","Alert","Beep & Alert","Central Notification"};
	for (int i = 0;i < 4;i++)
		menu->AddItem(new BMenuItem(strings[i],NULL));

	BMenuField *field = new BMenuField(rect,"notification_method","New E-mail Notification:",menu,B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	field->SetDivider(field->StringWidth(field->Label()) + 6);
	AddChild(field);

	ResizeToPreferred();
}		


void ConfigView::SetTo(BMessage *archive)
{
	int32 method = archive->FindInt32("notification_method");
	if (method <= 0)
		method = 1;
	
	if (BMenuField *field = (BMenuField *)FindView("notification_method"))
		field->Menu()->ItemAt(method-1)->SetMarked(true);
}


status_t ConfigView::Archive(BMessage *into,bool) const
{
	if (BMenuField *field = (BMenuField *)FindView("notification_method"))
	{
		int32 method = field->Menu()->IndexOf(field->Menu()->FindMarked()) + 1;
		if (into->ReplaceInt32("notification_method",method) != B_OK)
			into->AddInt32("notification_method",method);
	}

	return B_OK;
}

	
void ConfigView::GetPreferredSize(float *width, float *height)
{
	*width = 258;
	*height = ChildAt(0)->Bounds().Height() + 8;
}

