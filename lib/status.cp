/* StatusWindow - the status window while fetching/sending mails
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <StringView.h>
#include <Screen.h>
#include <String.h>
#include <StatusBar.h>

#include <stdio.h>
#include <assert.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT StatusWindow;
	class _EXPORT StatusView;
}
}

#include "status.h"
#include "MailSettings.h"

#include <MDRLanguage.h>

/*------------------------------------------------

StatusWindow

------------------------------------------------*/

using namespace Zoidberg;
using Mail::StatusWindow;
using Mail::StatusView;

// constructor
StatusWindow::StatusWindow(BRect rect, const char *name, uint32 s)
			: BWindow(rect, name, B_MODAL_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
			          B_NOT_CLOSABLE | B_NO_WORKSPACE_ACTIVATION | B_NOT_V_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE),
			  show(s),
			  default_is_hidden(false),
			  window_moved(0L)
{
	BRect frame(Bounds());
	frame.InsetBy(90.0 + 5.0, 5.0);

	BButton *button = new BButton(frame, "check_mail", MDR_DIALECT_CHOICE ("Check Mail Now","メールチェック"), new BMessage('mbth'),
								  B_FOLLOW_LEFT_RIGHT,
								  B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_NAVIGABLE);
	button->ResizeToPreferred();
	frame = button->Frame();
	
	button->ResizeTo(button->Bounds().Width(),25);
	button->SetTarget(be_app_messenger);

	frame.OffsetBy(0.0, frame.Height());
	frame.InsetBy(-90.0, 0.0);
	
	message_view = new BStringView(frame, "message_view", "",
								   B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
	message_view->SetAlignment(B_ALIGN_CENTER);
	message_view->SetText(MDR_DIALECT_CHOICE ("No new messages.","未読メッセージはありません"));
	float framewidth = frame.Width();
	message_view->ResizeToPreferred();
	message_view->ResizeTo(framewidth,message_view->Bounds().Height());
	frame = message_view->Frame();

	frame.InsetBy(-5.0, -5.0);
	frame.top = 0.0;

	default_view = new BBox(frame, "default_view", B_FOLLOW_LEFT_RIGHT,
							B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE_JUMP, B_PLAIN_BORDER);
	default_view->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	default_view->AddChild(button);
	default_view->AddChild(message_view);

	min_width = default_view->Bounds().Width();
	min_height = default_view->Bounds().Height();
	ResizeTo(min_width, min_height);
	SetSizeLimits(min_width, 2.0 * min_width, min_height, min_height);

	Mail::Settings general;
	if (general.InitCheck() == B_OK)
	{
		// set on-screen location

		frame = general.StatusWindowFrame();
		BScreen screen(this);
		if (screen.Frame().Contains(frame))
		{
			MoveTo(frame.LeftTop());
			if (frame.Width() >= min_width && frame.Height() >= min_height)
			{
				float x_off_set = frame.Width() - min_width;
				float y_off_set = 0; //---The height is constant

				ResizeBy(x_off_set, y_off_set);
				default_view->ResizeBy(x_off_set, y_off_set);
				button->ResizeBy(x_off_set, y_off_set);
				message_view->ResizeBy(x_off_set, y_off_set);
			}
		}
		// set workspace for window
		
		int32 workspace = general.StatusWindowWorkspaces();
		int32 workspacesCount = count_workspaces();
		uint32 workspacesMask = (workspacesCount > 31 ? 0 : 1L << workspacesCount) - 1;
		if ((workspacesMask & workspace) && (workspace != Workspaces()))
			SetWorkspaces(workspace);

		// set look

		SetBorderStyle(general.StatusWindowLook());
	}
	AddChild(default_view);

	window_frame = Frame();

	if (show == MD_SHOW_STATUS_WINDOW_ALWAYS)
		Show();
	else
	{
		Hide();
		Show();
	}
}

// destructor
StatusWindow::~StatusWindow()
{
	// remove all status_views, so we don't accidentally delete them
	while (StatusView *status_view = (StatusView *)stat_views.RemoveItem(0L))
		RemoveView(status_view);

	Mail::Settings general;
	if (general.InitCheck() == B_OK)
	{
		general.SetStatusWindowFrame(Frame());
		general.SetStatusWindowWorkspaces((int32)Workspaces());
		general.Save();
	}
}


void StatusWindow::FrameMoved(BPoint /*origin*/)
{
	if (last_workspace == current_workspace())
		window_frame = Frame();
}


void StatusWindow::WorkspaceActivated(int32 workspace, bool active)
{
	if (active)
	{
		MoveTo(window_frame.LeftTop());
		last_workspace = workspace;

		// make the window visible if the screen's frame doesn't contain it
		BScreen screen;
		if (screen.Frame().bottom < window_frame.top)
			MoveTo(window_frame.left-1, screen.Frame().bottom - window_frame.Height() - 4);
		if (screen.Frame().right < window_frame.left)
			MoveTo(window_frame.left-1, screen.Frame().bottom - window_frame.Height() - 4);
	}
}


// MessageReceived
void StatusWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case 'lkch':
		{
			int32 look;
			if (msg->FindInt32("StatusWindowLook",&look) == B_OK)
				SetBorderStyle(look);
			break;
		}
		case 'wsch':
		{
			int32 workspaces;
			if (msg->FindInt32("StatusWindowWorkSpace",&workspaces) != B_OK)
				break;
			if ((Workspaces() != B_ALL_WORKSPACES) && (workspaces != B_ALL_WORKSPACES))
				break;
			if (workspaces != Workspaces())
				SetWorkspaces(workspaces);
			break;
		}
		default:
			BWindow::MessageReceived(msg);
	}
}

// SetDefaultMessage
void StatusWindow::SetDefaultMessage(const BString& m)
{
	if (Lock())
	{
		message_view->SetText(m.String());
		Unlock();
	}
}

// NewStatusView
StatusView *StatusWindow::NewStatusView(const char *description,bool upstream) {
	if (!Lock()) return NULL;
	
	BRect rect = Bounds();
	rect.top = stat_views.CountItems() * (min_height + 1);
	rect.bottom = rect.top + min_height;
	StatusView *status = new StatusView(rect,description,upstream);
	status->window = this;
	Unlock();
	
	return status;
}

// ActuallyAddStatusView
void StatusWindow::ActuallyAddStatusView(StatusView *status) {
	if (!Lock())
		return;

	BRect rect = Bounds();
	rect.top = stat_views.CountItems() * (min_height + 1);
	rect.bottom = rect.top + min_height;
	
	status->MoveTo(rect.LeftTop());
	status->ResizeTo(rect.Width(),rect.Height());
	
	stat_views.AddItem((void *)status);
	
	status->Hide();
	AddChild(status);
	if (CountVisibleItems() == 1 && !default_is_hidden)
	{
		default_view->Hide();
		default_is_hidden = true;
	}
	status->Show();
	SetSizeLimits(10.0, 2000.0, 10.0, 2000.0);
	
	// if the window doesn't fit on screen anymore, move it
	BScreen screen;
	if (screen.Frame().bottom < Frame().top + rect.bottom)
	{
		MoveBy(0, Bounds().Height() - rect.bottom);
		window_moved++;
	}
	ResizeTo(rect.Width(), rect.bottom);
	
	if (show != MD_SHOW_STATUS_WINDOW_ALWAYS
		&& show != MD_SHOW_STATUS_WINDOW_NEVER
		&& CountVisibleItems() == 1)
	{
		SetFlags(Flags() | B_AVOID_FOCUS);
		Show();
		SetFlags(Flags() ^ B_AVOID_FOCUS);
	}
	Unlock();
}

// RemoveView
void StatusWindow::RemoveView(StatusView *view) {
	if (!view || !Lock()) return;
	
	int32 i = stat_views.IndexOf(view);
	if (i < 0) return;
	
	stat_views.RemoveItem((void *)view);
	if (RemoveChild(view))
	{
		// delete view; ?!?
		for (; StatusView *v = (StatusView *)stat_views.ItemAt(i); i++)
			v->MoveBy(0, -(min_height+1));
	}
	
	if (window_moved)
	{
		window_moved--;
		MoveBy(0, min_height + 1);
	}

	if (CountVisibleItems() == 0)
	{
		if (show != MD_SHOW_STATUS_WINDOW_NEVER && show != MD_SHOW_STATUS_WINDOW_ALWAYS)
		{
			while (!IsHidden())
				Hide();
		}
				
		if (default_is_hidden)
		{
			default_view->Show();
			default_is_hidden = false;
		}
		SetSizeLimits(min_width, 2.0 * min_width, min_height, min_height);
		ResizeTo(default_view->Frame().Width(), default_view->Frame().Height());

		be_app->PostMessage('stwg');
	}
	else
		ResizeTo(Bounds().Width(), stat_views.CountItems() * min_height - 1);

	Unlock();
}


int32 StatusWindow::CountVisibleItems()
{
	if (show != MD_SHOW_STATUS_WINDOW_WHEN_SENDING)
		return stat_views.CountItems();

	int32 count = 0;
	for (int32 i = stat_views.CountItems();i-- > 0;)
	{
		StatusView *view = (StatusView *)stat_views.ItemAt(i);
		if (view->is_upstream)
			count++;
	}
	return count;
}

// HasItems
bool StatusWindow::HasItems(void) {
	return (CountVisibleItems() > 0);
}

// SetShowCriterion
void StatusWindow::SetShowCriterion(uint32 when)
{
	if (!Lock())
		return;

	show = when;
	if (show == MD_SHOW_STATUS_WINDOW_ALWAYS
		|| (show != MD_SHOW_STATUS_WINDOW_NEVER && HasItems()))
	{
		while (IsHidden())
			Show();
	} else {
		while (!IsHidden())
			Hide();
	}
	Unlock();
}

// SetBorderStyle
void StatusWindow::SetBorderStyle(int32 look)
{
	switch (look)
	{
		case MD_STATUS_LOOK_TITLED:
			SetLook(B_TITLED_WINDOW_LOOK);
			break;
		case MD_STATUS_LOOK_FLOATING:
			SetLook(B_FLOATING_WINDOW_LOOK);
			break;
		case MD_STATUS_LOOK_THIN_BORDER:
			SetLook(B_BORDERED_WINDOW_LOOK);
			break;
		case MD_STATUS_LOOK_NO_BORDER:
			SetLook(B_NO_BORDER_WINDOW_LOOK);
			break;

		case MD_STATUS_LOOK_NORMAL_BORDER:
		default:
			SetLook(B_MODAL_WINDOW_LOOK);
	}
}


//	#pragma mark -
/*------------------------------------------------

StatusView

------------------------------------------------*/

// constructor
StatusView::StatusView(BRect rect, const char *description,bool upstream)
		  : BBox(rect, description, B_FOLLOW_LEFT_RIGHT,
		  		 B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE_JUMP,
		  		 B_PLAIN_BORDER)
{
	status = new BStatusBar(BRect(5, 5, Bounds().right - 5, Bounds().bottom - 5),
							"status_bar", description, "");
	status->SetResizingMode(B_FOLLOW_ALL_SIDES);
	status->SetBarHeight(12);

	if (!upstream) {
		const rgb_color downstreamColor = {48,176,48,255};	// upstream was: {255,100,50,255}
		status->SetBarColor(downstreamColor);
	}
	AddChild(status);

	items_now = 0;
	total_items = 0;
	pre_text[0] = 0;
	is_upstream = upstream;
}

// destructor
StatusView::~StatusView()
{
}

// AddProgress
void StatusView::AddProgress(int32 how_much) {
	AddSelfToWindow();
	
	if (LockLooper())
	{
		if (status->CurrentValue() == 0)
			strcpy(pre_text,status->TrailingText());
		char final[80];
		if (by_bytes) {
			sprintf(final,"%.1f / %.1f kb (%d / %d messages)",float(float(status->CurrentValue() + how_much) / 1024),float(float(status->MaxValue()) / 1024),(int)items_now+1,(int)total_items);
			status->Update(how_much,NULL,final);
		} else {
			sprintf(final,"%d / %d messages",(int)items_now,(int)total_items);
			status->Update(how_much,NULL,final);
		}
		UnlockLooper();
	}
}

// SetMessage
void StatusView::SetMessage(const char *msg) {
	AddSelfToWindow();
	
	if (LockLooper())
	{
		status->SetTrailingText(msg);
		UnlockLooper();
	}
}

void StatusView::Reset(bool hide) {
	if (LockLooper())
	{
		char old[255];
		if ((pre_text[0] == 0) && !hide)
			strcpy(pre_text,status->TrailingText());
		if (hide)
			pre_text[0] = 0;
			
		strcpy(old,status->Label());
		status->Reset(old);
		status->SetTrailingText(pre_text);
		status->Draw(status->Bounds());
		pre_text[0] = 0;
		total_items = 0;
		items_now = 0;
		UnlockLooper();
	}
	if (hide)
		if (Window()) window->RemoveView(this);
}

// SetMaximum
void StatusView::SetMaximum(int32 max_bytes) {
	AddSelfToWindow();
	
	if (LockLooper())
	{
		if (max_bytes < 0) {
			status->SetMaxValue(total_items);
			by_bytes = false;
		} else {
			status->SetMaxValue(max_bytes);
			by_bytes = true;
		}
		UnlockLooper();
	}
}

// SetTotalItems
void StatusView::SetTotalItems(int32 items) {
	AddSelfToWindow();
	total_items = items;
}

int32 StatusView::CountTotalItems() {
	return total_items;
}

// AddItem
void StatusView::AddItem(void) {
	AddSelfToWindow();
	items_now++;
	
	if (!by_bytes)
		AddProgress(1);
}

void StatusView::AddSelfToWindow() {
	if (Window() != NULL)
		return;
	
	window->ActuallyAddStatusView(this);
}

//--------------------------------------------------------------------------
//	#pragma mark -

_EXPORT void Mail::ShowAlert(const char *title, const char *body, const char *button, alert_type type)
{
printf("Alert (%s): %s [%s]\n",title,body,button);
	BAlert *alert = new BAlert(title,body,button,NULL,NULL,B_WIDTH_AS_USUAL,type);
	alert->SetFeel(B_NORMAL_WINDOW_FEEL);
	alert->Go(NULL);
}

