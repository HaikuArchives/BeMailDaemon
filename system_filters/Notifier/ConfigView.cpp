/* ConfigView - the configuration view for the Notifier filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include "ConfigView.h"

#include <CheckBox.h>
#include <String.h>
#include <Message.h>

#include <MailAddon.h>

enum {
	do_beep = 1,
	alert = 2,
	blink_leds = 4
};

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
	
	BRect frame = rect;
	frame.right /= 4;
	AddChild(new BCheckBox(frame,"beep","Beep",NULL));
	frame.left = frame.right;
	frame.right *= 2;
	AddChild(new BCheckBox(frame,"alert","Alert",NULL));
	rect.top += itemHeight;
	rect.bottom += itemHeight;
	
	AddChild(new BCheckBox(rect,"blink","Blink Keyboard LEDs",NULL));
	ResizeToPreferred();
}		


void ConfigView::SetTo(BMessage *archive)
{
	int32 method = archive->FindInt32("notification_method");
	if (method <= 0)
		method = 1;
	
	if (method & do_beep)
		((BCheckBox *)(FindView("beep")))->SetValue(B_CONTROL_ON);
	if (method & alert)
		((BCheckBox *)(FindView("alert")))->SetValue(B_CONTROL_ON);
	if (method & blink_leds)
		((BCheckBox *)(FindView("blink")))->SetValue(B_CONTROL_ON);
}


status_t ConfigView::Archive(BMessage *into,bool) const
{
	int32 method = 0;
	if (((BCheckBox *)(FindView("beep")))->Value() == B_CONTROL_ON)
		method |= do_beep;
	if (((BCheckBox *)(FindView("alert")))->Value() == B_CONTROL_ON)
		method |= alert;
	if (((BCheckBox *)(FindView("blink")))->Value() == B_CONTROL_ON)
		method |= blink_leds;
		
	if (into->ReplaceInt32("notification_method",method) != B_OK)
		into->AddInt32("notification_method",method);

	return B_OK;
}

	
void ConfigView::GetPreferredSize(float *width, float *height)
{
	*width = 258;
	*height = ChildAt(0)->Bounds().Height()*2 + 8;
}

BView* instantiate_config_panel(BMessage *settings,BMessage *)
{
	ConfigView *view = new ConfigView();
	view->SetTo(settings);

	return view;
}
