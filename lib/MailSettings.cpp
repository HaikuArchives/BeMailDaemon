#include <Message.h>
#include <FindDirectory.h>
#include <Directory.h>
#include <File.h>
#include <Entry.h>
#include <Path.h>
#include <String.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

class _EXPORT MailSettings;

#include "MailSettings.h"
#include "NumailKit.h"

MailSettings::MailSettings()
{
	Reload();
}

MailSettings::~MailSettings()
{
}

status_t MailSettings::InitCheck() const
{
	return B_OK;
}


status_t MailSettings::Save(bigtime_t timeout)
{
	status_t ret;
	//
	// Find chain-saving directory
	//
	
	BPath path;
	ret = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't find user settings directory: %s\n",
			strerror(ret));
		return ret;
	}
	
	path.Append("Mail");
	return WriteMessageFile(data,path,"new_mail_daemon");
}

status_t MailSettings::Reload()
{
	status_t ret;
	
	BPath path;
	ret = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't find user settings directory: %s\n",
			strerror(ret));
		return ret;
	}
	
	path.Append("Mail/new_mail_daemon");
	
	// open
	BFile settings(path.Path(),B_READ_ONLY);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't open settings file '%s': %s\n",
			path.Path(), strerror(ret));
		return ret;
	}
	
	// read settings
	BMessage tmp;
	ret = tmp.Unflatten(&settings);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't read settings from '%s': %s\n",
			path.Path(), strerror(ret));
		return ret;
	}
	
	// clobber old settings
	data = tmp;
	return B_OK;
}


// Chain methods

//
// To do
//
MailChain* MailSettings::NewChain()
{
	// attempted solution: use time(NULL) and hope it's unique. Is there a better idea?
	// note that two chains in two second is quite possible. how to fix this?
	// maybe we could | in some bigtime_t as well. hrrm...
	
	
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("Mail/chains");
	BDirectory chain_dir(path.Path());
	BDirectory outbound_dir(&chain_dir,"outbound"), inbound_dir(&chain_dir,"inbound");
	
	chain_dir.Lock(); //---------Try to lock the directory
	
	int32 id = -1; //-----When inc'ed, we start with 0----
	chain_dir.ReadAttr("last_issued_chain_id",B_INT32_TYPE,0,&id,sizeof(id));
	
	BString string_id;
	
	do {
		id++;
		string_id = "";
		string_id << id;
	} while ((outbound_dir.Contains(string_id.String())) || (inbound_dir.Contains(string_id.String())));
		
	
	chain_dir.WriteAttr("last_issued_chain_id",B_INT32_TYPE,0,&id,sizeof(id));
	
	return new MailChain(id);
}
//
// Done
//

MailChain* MailSettings::GetChain(uint32 id)
{
	return new MailChain(id);
}

status_t MailSettings::InboundChains(BList *list)
{
	BPath path;
	status_t ret = B_OK;
	
	ret = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't find user settings directory: %s\n",
			strerror(ret));
		return ret;
	}
	
	path.Append("Mail/chains/inbound");
	BDirectory chain_dir(path.Path());
	entry_ref ref;
	
	while (chain_dir.GetNextRef(&ref)==B_OK)
	{
		char *end;
		uint32 id = strtoul(ref.name, &end, 10);
		
		if (id && (!end || *end == '\0'))
			list->AddItem((void*)new MailChain(id));
	}
	
	return ret;
}

status_t MailSettings::OutboundChains(BList *list)
{
	BPath path;
	status_t ret = B_OK;
	
	ret = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (ret != B_OK)
	{
		fprintf(stderr, "Couldn't find user settings directory: %s\n",
			strerror(ret));
		return ret;
	}
	
	path.Append("Mail/chains/outbound");
	BDirectory chain_dir(path.Path());
	entry_ref ref;
	
	while (chain_dir.GetNextRef(&ref)==B_OK)
	{
		char *end;
		uint32 id = strtoul(ref.name, &end, 10);
		if (id && (!end || *end == '\0'))
			list->AddItem((void*)new MailChain(id));
	}
	
	return ret;
}


// Global settings
int32 MailSettings::WindowFollowsCorner()
{
	return data.FindInt32("WindowFollowsCorner");
}
void MailSettings::SetWindowFollowsCorner(int32 which_corner)
{
	if (data.ReplaceInt32("WindowFollowsCorner",which_corner))
		data.AddInt32("WindowFollowsCorner",which_corner);
}

uint32 MailSettings::ShowStatusWindow()
{
	return data.FindInt32("ShowStatusWindow");
}
void MailSettings::SetShowStatusWindow(uint32 mode)
{
	if (data.ReplaceInt32("ShowStatusWindow",mode))
		data.AddInt32("ShowStatusWindow",mode);
}

bool MailSettings::DaemonAutoStarts()
{
	return data.FindBool("DaemonAutoStarts");
}
void MailSettings::SetDaemonAutoStarts(bool does_it)
{
	if (data.ReplaceBool("DaemonAutoStarts",does_it))
		data.AddBool("DaemonAutoStarts",does_it);
}

BRect MailSettings::ConfigWindowFrame()
{
	return data.FindRect("ConfigWindowFrame");
}
void MailSettings::SetConfigWindowFrame(BRect frame)
{
	if (data.ReplaceRect("ConfigWindowFrame",frame))
		data.AddRect("ConfigWindowFrame",frame);
}

BRect MailSettings::StatusWindowFrame()
{
	return data.FindRect("StatusWindowFrame");
}
void MailSettings::SetStatusWindowFrame(BRect frame)
{
	if (data.ReplaceRect("StatusWindowFrame",frame))
		data.AddRect("StatusWindowFrame",frame);
}

int32 MailSettings::StatusWindowWorkSpace()
{
	return data.FindInt32("StatusWindowWorkSpace");
}
void MailSettings::SetStatusWindowWorkSpace(int32 workspace)
{
	if (data.ReplaceInt32("StatusWindowWorkSpace",workspace))
		data.AddInt32("StatusWindowWorkSpace",workspace);
}

int32 MailSettings::StatusWindowLook()
{
	return data.FindInt32("StatusWindowLook");
}
void MailSettings::SetStatusWindowLook(int32 look)
{
	if (data.ReplaceInt32("StatusWindowLook",look))
		data.AddInt32("StatusWindowLook",look);
}

bigtime_t MailSettings::AutoCheckInterval() {
	bigtime_t value = B_INFINITE_TIMEOUT;
	data.FindInt64("AutoCheckInterval",&value);
	return value;
}

void MailSettings::SetAutoCheckInterval(bigtime_t interval) {
	if (data.ReplaceInt64("AutoCheckInterval",interval))
		data.AddInt64("AutoCheckInterval",interval);
}

bool MailSettings::CheckOnlyIfPPPUp() {
	return data.FindBool("CheckOnlyIfPPPUp");
}

void MailSettings::SetCheckOnlyIfPPPUp(bool yes) {
	if (data.ReplaceBool("CheckOnlyIfPPPUp",yes))
		data.AddBool("CheckOnlyIfPPPUp",yes);
}

uint32 MailSettings::DefaultOutboundChainID() {
	return data.FindInt32("DefaultOutboundChainID");
}

void MailSettings::SetDefaultOutboundChainID(uint32 to) {
	if (data.ReplaceInt32("DefaultOutboundChainID",to))
		data.AddInt32("DefaultOutboundChainID",to);
	
	//------Note that once we have our own BMailMessage, this can be swiftly relegated
	//      to the dustbin of history
	
	/*--------Hack!!! Hack!!! Hack!!!------------*/
	MailChain new_acc(to);
	create_directory("/boot/home/config/settings/Mail/MailPop/",0777);
	create_directory("/boot/home/config/settings/Mail/MailSmtp/",0777);
	BFile old_daemon("/boot/home/config/settings/Mail/MailPop/MailPop1",B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);

	BMessage old_settings;
	old_settings.AddString("Version_info","V1.00");
	old_settings.AddString("pop_user_name",B_EMPTY_STRING);
	old_settings.AddString("pop_password",B_EMPTY_STRING);
	old_settings.AddString("pop_host",B_EMPTY_STRING);
	old_settings.AddString("pop_real_name",new_acc.MetaData()->FindString("real_name"));
	old_settings.AddString("pop_reply_to",new_acc.MetaData()->FindString("reply_to"));
	old_settings.AddInt32("pop_days",0);
	old_settings.AddInt32("pop_interval",0);
	old_settings.AddInt32("pop_start",0);
	old_settings.AddInt32("pop_end",0);
	
	old_settings.Flatten(&old_daemon);
	
	old_daemon.SetTo("/boot/home/config/settings/Mail/MailSmtp/MailSmtp1",B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
	old_settings.MakeEmpty();
	
	old_settings.AddString("Version_info","V1.00");
	old_settings.AddString("smtp_host","foo.bar.com"/* B_EMPTY_STRING will trigger an error */);
	
	old_settings.Flatten(&old_daemon);
	/*-------Phew! Hackage is done!-------------*/
}