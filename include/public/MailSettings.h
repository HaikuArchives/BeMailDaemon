#ifndef ZOIDBERG_MAIL_SETTINGS_H
#define ZOIDBERG_MAIL_SETTINGS_H
/* Settings - the mail daemon's settings
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Archivable.h>
#include <List.h>
#include <Message.h>

class BPath;


typedef enum {
	inbound,
	outbound
} chain_direction;

typedef enum
{
	MD_SHOW_STATUS_WINDOW_NEVER         = 0,
	MD_SHOW_STATUS_WINDOW_WHEN_FETCHING = 1,
	MD_SHOW_STATUS_WINDOW_ALWAYS        = 2
} mail_status_window_option;

typedef enum
{
	MD_STATUS_LOOK_TITLED                = 0,
	MD_STATUS_LOOK_NORMAL_BORDER         = 1,
	MD_STATUS_LOOK_FLOATING              = 2,
	MD_STATUS_LOOK_THIN_BORDER           = 3,
	MD_STATUS_LOOK_NO_BORDER             = 4
} mail_status_window_look;


namespace Zoidberg {
namespace Mail {

class StatusWindow;
class Chain;

Chain* NewChain();
Chain* GetChain(uint32 id);

status_t OutboundChains(BList *list);
status_t InboundChains(BList *list);

class Chain : public BArchivable
{
  public:
	Chain(uint32 id);
	Chain(BMessage*);
	virtual ~Chain();
	
	virtual status_t Archive(BMessage*,bool) const;
	static BArchivable* Instantiate(BMessage*);
	
	status_t Save(bigtime_t timeout = B_INFINITE_TIMEOUT);
	status_t Delete() const;
	status_t Reload();
	status_t InitCheck() const;
	
	uint32 ID() const;
	
	chain_direction ChainDirection() const;
	void SetChainDirection(chain_direction);
	
	const char *Name() const;
	status_t SetName(const char*);
	
	BMessage *MetaData() const;
	
	// "Filter" below refers to the settings message for a MailFilter
	int32 CountFilters() const;
	status_t GetFilter(int32 index, BMessage* out_settings, entry_ref *addon = NULL) const;
	status_t SetFilter(int32 index, const BMessage&, const entry_ref&);
	
	status_t AddFilter(const BMessage&, const entry_ref&); // at end
	status_t AddFilter(int32 index, const BMessage&, const entry_ref&);
	status_t RemoveFilter(int32 index);
	
	void RunChain(StatusWindow *window,
		bool async = true,
		bool save_when_done = true,
		bool delete_when_done = false);
	
  private:
	status_t Path(BPath *path) const;
	status_t Load(BMessage*);
	
	int32 id;
	char name[B_FILE_NAME_LENGTH];
	BMessage *meta_data;
	
	status_t _err;

  	chain_direction direction;

	int32 settings_ct, addons_ct;  
	BList filter_settings;
	BList filter_addons;
	
	uint32 _reserved[5];
};

class Settings
{
  public:
	Settings();
	~Settings();
	
	status_t Save(bigtime_t timeout = B_INFINITE_TIMEOUT);
	status_t Reload();
	status_t InitCheck() const;
	
	// Global settings
	int32 WindowFollowsCorner();
	void SetWindowFollowsCorner(int32 which_corner);
	
	uint32 ShowStatusWindow();
	void SetShowStatusWindow(uint32 mode);
	
	bool DaemonAutoStarts();
	void SetDaemonAutoStarts(bool does_it);

	void SetConfigWindowFrame(BRect frame);
	BRect ConfigWindowFrame();

	void SetStatusWindowFrame(BRect frame);
	BRect StatusWindowFrame();

	int32 StatusWindowWorkspaces();
	void SetStatusWindowWorkspaces(int32 workspaces);

	int32 StatusWindowLook();
	void SetStatusWindowLook(int32 look);
	
	bigtime_t AutoCheckInterval();
	void SetAutoCheckInterval(bigtime_t);
	
	bool CheckOnlyIfPPPUp();
	void SetCheckOnlyIfPPPUp(bool yes);
	
	uint32 DefaultOutboundChainID();
	void SetDefaultOutboundChainID(uint32 to);

  private:
	BMessage data;
	uint32 _reserved[4];
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_SETTINGS_H */
