#ifndef ZOIDBERG_STATUS_WINDOW_H
#define ZOIDBERG_STATUS_WINDOW_H
/* StatusWindow - the status window while fetching/sending mails
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Window.h>
#include <Box.h>
#include <List.h>
#include <Alert.h>
#include <StatusBar.h>

class BStringView;


namespace Zoidberg {
namespace Mail {

class StatusView;

class StatusWindow : public BWindow {
	public:
						StatusWindow(BRect rect, const char *name, uint32 show_when);
						~StatusWindow();

				void	MessageReceived(BMessage *msg);

	Mail::StatusView	*NewStatusView(const char *description, bool upstream);
				void	RemoveView(StatusView *view);
		
				bool	HasItems(void);
				void	SetShowCriterion(uint32);
				void	SetDefaultMessage(const BString& m);

	private:
		friend	class	Mail::StatusView;

				void	SetBorderStyle(int32 look);
				void	ActuallyAddStatusView(StatusView *status);

		BList			stat_views;
		uint32			show;
		BView			*default_view;
		BStringView		*message_view;
		float			min_width;
		float			min_height;
		bool			default_is_hidden;
		int32			window_moved;
};

class StatusView : public BBox {
	public:
				void	AddProgress(int32 how_much);
				void	SetMessage(const char *msg);
				void	SetMaximum(int32 max_bytes);
				int32	CountTotalItems();
				void	SetTotalItems(int32 items);
				void	AddItem(void);
		
		virtual			~StatusView();

	private:
		friend class	Mail::StatusWindow;
		
						StatusView(BRect rect,const char *description);
				void	AddSelfToWindow();
		
		BStatusBar		*status;
		Mail::StatusWindow	*window;
		int32			items_now;
		int32			total_items;
};

extern void ShowAlert(const char *title, const char *body, const char *button = "Ok",
		alert_type type = B_INFO_ALERT);

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_STATUS_WINDOW_H */
