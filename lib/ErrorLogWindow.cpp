#include <ScrollView.h>
#include <TextView.h>
#include <limits.h>

#include "ErrorLogWindow.h"

rgb_color white = {255,255,255,255};
rgb_color notwhite = {255,255,200,255};

class Error : public BView {
	public:
		Error(BRect rect,alert_type type,const char *message,rgb_color bkg);
		
		void GetPreferredSize(float *width, float *height);
		void Draw(BRect updateRect);
		void FrameResized(float w, float h);
	private:
		alert_type type;
};

class ErrorPanel : public BView {
	public:
		ErrorPanel(BRect rect) : BView(rect,"ErrorScrollPanel",B_FOLLOW_ALL_SIDES,B_DRAW_ON_CHILDREN | B_FRAME_EVENTS), alerts_displayed(0), add_next_at(0) {}
		
		void GetPreferredSize(float *width, float *height) {
			*width = Bounds().Width();
			*height = add_next_at;
		}
		
		void TargetedByScrollView(BScrollView *scroll_view) { scroll = scroll_view; /*scroll->ScrollBar(B_VERTICAL)->SetRange(0,add_next_at);*/ }
		void FrameResized(float w, float /*h*/) {
			if (w == Frame().Width())
				return;
				
			add_next_at = 0;
			for (int32 i = 0; i < CountChildren(); i++) {
				ChildAt(i)->MoveTo(BPoint(0,add_next_at));
				ChildAt(i)->ResizeTo(w,ChildAt(i)->Frame().Height());
				ChildAt(i)->ResizeToPreferred();
				add_next_at += ChildAt(i)->Bounds().Height();
			}
			ResizeTo(w,add_next_at);				
		}
		
		int32 alerts_displayed;
		float add_next_at;
		BScrollView *scroll;
};

ErrorLogWindow::ErrorLogWindow(BRect rect, const char *name, window_type type) : BWindow(rect,name,type,B_NO_WORKSPACE_ACTIVATION | B_ASYNCHRONOUS_CONTROLS) {
	rect = Bounds();
	rect.right -= B_V_SCROLL_BAR_WIDTH;
	//rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
	
	view = new ErrorPanel(rect);
	AddChild(new BScrollView("ErrorScroller",view,B_FOLLOW_ALL_SIDES,0,false,true));
}

void ErrorLogWindow::AddError(alert_type type,const char *message) {
	Lock();
	ErrorPanel *panel = (ErrorPanel *)(view);
	Error *new_error = new Error(BRect(0,panel->add_next_at,panel->Bounds().right,panel->add_next_at+1),type,message,
								 (panel->alerts_displayed++ % 2 == 0) ? white : notwhite);
	new_error->ResizeToPreferred();
	panel->add_next_at += new_error->Bounds().Height();
	panel->AddChild(new_error);
	panel->ResizeToPreferred();
	if (panel->add_next_at > Frame().Height())
		panel->scroll->ScrollBar(B_VERTICAL)->SetRange(0,panel->add_next_at - Frame().Height());
	else
		panel->scroll->ScrollBar(B_VERTICAL)->SetRange(0,0);
	if (IsHidden())
		Show();
	Unlock();
}
	

bool ErrorLogWindow::QuitRequested() {
	Hide();
	while (view->CountChildren() != 0)
		view->RemoveChild(view->ChildAt(0));
	ErrorPanel *panel = (ErrorPanel *)(view);
	panel->add_next_at = 0;
	panel->alerts_displayed = 0;
	
	view->ResizeToPreferred();
	return false;
}

void ErrorLogWindow::FrameResized(float new_width, float new_height) {
	ErrorPanel *panel = (ErrorPanel *)(view);
	panel->ResizeTo(new_width,panel->add_next_at);
	panel->FrameResized(new_width-B_V_SCROLL_BAR_WIDTH,panel->add_next_at);
	panel->Invalidate();
	if (panel->add_next_at > new_height)
		panel->scroll->ScrollBar(B_VERTICAL)->SetRange(0,panel->add_next_at - new_height);
	else
		panel->scroll->ScrollBar(B_VERTICAL)->SetRange(0,0);
}

Error::Error(BRect rect,alert_type atype,const char *message,rgb_color bkg) : BView(rect,"error",B_FOLLOW_LEFT | B_FOLLOW_RIGHT | B_FOLLOW_TOP,B_NAVIGABLE | B_WILL_DRAW | B_FRAME_EVENTS), type(atype) {
	SetViewColor(bkg);
	SetLowColor(bkg);
	BTextView *view = new BTextView(BRect(20,0,rect.Width(),rect.Height()),"error_display",BRect(0,3,rect.Width() - 20 - 3,LONG_MAX),B_FOLLOW_ALL_SIDES);
	view->SetLowColor(bkg);
	view->SetViewColor(bkg);
	view->SetText(message);
	view->MakeSelectable(true);
	view->MakeEditable(false);
	float height,width;
	width = view->Frame().Width();
	height = view->TextHeight(0,view->CountLines()) + 3;
	view->ResizeTo(width,height);
	AddChild(view);
}	

void Error::GetPreferredSize(float *width, float *height) {
	*width = FindView("error_display")->Frame().Width() + 20;
	*height = ((BTextView *)(FindView("error_display")))->TextHeight(0,LONG_MAX) + 3;
}
	
void Error::Draw(BRect updateRect) {
	FillRect(updateRect,B_SOLID_LOW);
}

void Error::FrameResized(float w, float h) {
	FindView("error_display")->ResizeTo(w-20,h);
	((BTextView *)(FindView("error_display")))->SetTextRect(BRect(0,3,w-20,h));
}
