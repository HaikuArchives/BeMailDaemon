#ifndef ZOIDBERG_STATUS_WINDOW_H
#define ZOIDBERG_STATUS_WINDOW_H
/* StatusWindow - the status window while fetching/sending mails
**
** Copyright 2001-2003 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Window.h>
#include <Box.h>
#include <List.h>
#include <Alert.h>

class BStatusBar;
class BStringView;


namespace Zoidberg {
namespace Mail {

class StatusView;

class StatusWindow : public BWindow {
	public:
		StatusWindow(BRect rect, const char *name, uint32 show_when);
		~StatusWindow();

		virtual	void FrameMoved(BPoint origin);
		virtual void WorkspaceActivated(int32 workspace, bool active);
		virtual void MessageReceived(BMessage *msg);

		StatusView *NewStatusView(const char *description, bool upstream);
		void	RemoveView(StatusView *view);
		int32	CountVisibleItems();

		bool	HasItems(void);
		void	SetShowCriterion(uint32);
		void	SetDefaultMessage(const BString &message);

	private:
		friend class Mail::StatusView;

		void	SetBorderStyle(int32 look);
		void	ActuallyAddStatusView(StatusView *status);

		BList		fStatusViews;
		uint32		fShowMode;
		BView		*fDefaultView;
		BStringView	*fMessageView;
		float		fMinWidth;
		float		fMinHeight;
		int32		fWindowMoved;
		int32		fLastWorkspace;
		BRect		fFrame;
		
		uint32		_reserved[5];
};

class StatusView : public BBox {
	public:
				void	AddProgress(int32 how_much);
				void	SetMessage(const char *msg);
				void	SetMaximum(int32 max_bytes);
				int32	CountTotalItems();
				void	SetTotalItems(int32 items);
				void	AddItem(void);
				void	Reset(bool hide = true);
		
		virtual			~StatusView();

	private:
		friend class	Mail::StatusWindow;
		
						StatusView(BRect rect,const char *description,bool upstream);
				void	AddSelfToWindow();
		
		BStatusBar		*status;
		Mail::StatusWindow	*window;
		int32			items_now;
		int32			total_items;
		bool			is_upstream;
		bool			by_bytes;
		char 			pre_text[255];

		uint32			_reserved[5];
};

extern void ShowAlert(const char *title, const char *body, const char *button = "Ok",
		alert_type type = B_INFO_ALERT);

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_STATUS_WINDOW_H */
