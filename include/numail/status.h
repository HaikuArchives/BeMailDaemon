// status.h

#ifndef STATUS_WINDOW_H
#define STATUS_WINDOW_H

#include <Window.h>
#include <Box.h>
#include <List.h>
#include <StatusBar.h>

class StatusView;
class BStringView;

class StatusWindow : public BWindow {
	public:
						StatusWindow(BRect rect, const char *name, uint32 show_when);
						~StatusWindow();

				void	MessageReceived(BMessage *msg);

			StatusView	*NewStatusView(const char *description, bool upstream);
				void	RemoveView(StatusView *view);
		
				bool	HasItems(void);
				void	SetShowCriterion(uint32);
				void	SetDefaultMessage(const char* m);

	private:
		friend	class	StatusView;

				void	SetBorderStyle(int32 look);
				void	ActuallyAddStatusView(StatusView *status);

		BList			stat_views;
		uint32			show;
		BView			*default_view;
		BStringView		*message_view;
		float			min_width;
		float			min_height;
		bool			default_is_hidden;
};

class StatusView : public BBox {
	public:
				void	AddProgress(int32 how_much);
				void	SetMessage(const char *msg);
				void	SetMaximum(int32 max_bytes);
				void	SetTotalItems(int32 items);
				void	AddItem(void);
		
		virtual			~StatusView();

	private:
		friend class	StatusWindow;
		
						StatusView(BRect rect,const char *description);
				void	AddSelfToWindow();
		
		BStatusBar		*status;
		StatusWindow	*window;
		int32			items_now;
		int32			total_items;
};

#endif // STATUS_WINDOW_H
