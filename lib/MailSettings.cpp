/* Settings - the mail daemon's settings
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>
#include <FindDirectory.h>
#include <Directory.h>
#include <File.h>
#include <Entry.h>
#include <Path.h>
#include <String.h>
#include <Messenger.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT Settings;
}
}

#include <MailSettings.h>
#include <NumailKit.h>


namespace Zoidberg {
namespace Mail {

Settings::Settings()
{
	Reload();
}

Settings::~Settings()
{
}

status_t Settings::InitCheck() const
{
	return B_OK;
}


status_t Settings::Save(bigtime_t /*timeout*/)
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
	
	status_t result = WriteMessageFile(data,path,"new_mail_daemon");
	if (result < B_OK)
		return result;
		
	BMessenger("application/x-vnd.Be-POST").SendMessage('mrrs');
	
	return B_OK;
}

status_t Settings::Reload()
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
	ret = settings.InitCheck();
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
_EXPORT Mail::Chain* Mail::NewChain()
{
	// attempted solution: use time(NULL) and hope it's unique. Is there a better idea?
	// note that two chains in two second is quite possible. how to fix this?
	// maybe we could | in some bigtime_t as well. hrrm...

	// This is to fix a problem with generating the correct id for chains.
	// Basically if the chains dir does not exist, the first time you create
	// an account both the inbound and outbound chains will be called 0.
	create_directory("/boot/home/config/settings/Mail/chains",0777);	
	
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
	
	return new Mail::Chain(id);
}
//
// Done
//

_EXPORT Mail::Chain* Mail::GetChain(uint32 id)
{
	return new Mail::Chain(id);
}

_EXPORT status_t Mail::InboundChains(BList *list)
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
		
		if (!end || *end == '\0')
			list->AddItem((void*)new Mail::Chain(id));
	}
	
	return ret;
}

_EXPORT status_t Mail::OutboundChains(BList *list)
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
		if (!end || *end == '\0')
			list->AddItem((void*)new Mail::Chain(id));
	}
	
	return ret;
}


// Global settings
int32 Settings::WindowFollowsCorner()
{
	return data.FindInt32("WindowFollowsCorner");
}
void Settings::SetWindowFollowsCorner(int32 which_corner)
{
	if (data.ReplaceInt32("WindowFollowsCorner",which_corner))
		data.AddInt32("WindowFollowsCorner",which_corner);
}

uint32 Settings::ShowStatusWindow()
{
	return data.FindInt32("ShowStatusWindow");
}
void Settings::SetShowStatusWindow(uint32 mode)
{
	if (data.ReplaceInt32("ShowStatusWindow",mode))
		data.AddInt32("ShowStatusWindow",mode);
}

bool Settings::DaemonAutoStarts()
{
	return data.FindBool("DaemonAutoStarts");
}
void Settings::SetDaemonAutoStarts(bool does_it)
{
	if (data.ReplaceBool("DaemonAutoStarts",does_it))
		data.AddBool("DaemonAutoStarts",does_it);
}

BRect Settings::ConfigWindowFrame()
{
	return data.FindRect("ConfigWindowFrame");
}
void Settings::SetConfigWindowFrame(BRect frame)
{
	if (data.ReplaceRect("ConfigWindowFrame",frame))
		data.AddRect("ConfigWindowFrame",frame);
}

BRect Settings::StatusWindowFrame()
{
	return data.FindRect("StatusWindowFrame");
}
void Settings::SetStatusWindowFrame(BRect frame)
{
	if (data.ReplaceRect("StatusWindowFrame",frame))
		data.AddRect("StatusWindowFrame",frame);
}

int32 Settings::StatusWindowWorkspaces()
{
	return data.FindInt32("StatusWindowWorkSpace");
}
void Settings::SetStatusWindowWorkspaces(int32 workspace)
{
	if (data.ReplaceInt32("StatusWindowWorkSpace",workspace))
		data.AddInt32("StatusWindowWorkSpace",workspace);

	BMessage msg('wsch');
	msg.AddInt32("StatusWindowWorkSpace",workspace);
	BMessenger("application/x-vnd.Be-POST").SendMessage(&msg);
}

int32 Settings::StatusWindowLook()
{
	return data.FindInt32("StatusWindowLook");
}
void Settings::SetStatusWindowLook(int32 look)
{
	if (data.ReplaceInt32("StatusWindowLook",look))
		data.AddInt32("StatusWindowLook",look);
		
	BMessage msg('lkch');
	msg.AddInt32("StatusWindowLook",look);
	BMessenger("application/x-vnd.Be-POST").SendMessage(&msg);
}

bigtime_t Settings::AutoCheckInterval() {
	bigtime_t value = B_INFINITE_TIMEOUT;
	data.FindInt64("AutoCheckInterval",&value);
	return value;
}

void Settings::SetAutoCheckInterval(bigtime_t interval) {
	if (data.ReplaceInt64("AutoCheckInterval",interval))
		data.AddInt64("AutoCheckInterval",interval);
}

bool Settings::CheckOnlyIfPPPUp() {
	return data.FindBool("CheckOnlyIfPPPUp");
}

void Settings::SetCheckOnlyIfPPPUp(bool yes) {
	if (data.ReplaceBool("CheckOnlyIfPPPUp",yes))
		data.AddBool("CheckOnlyIfPPPUp",yes);
}

uint32 Settings::DefaultOutboundChainID() {
	return data.FindInt32("DefaultOutboundChainID");
}

void Settings::SetDefaultOutboundChainID(uint32 to) {
	if (data.ReplaceInt32("DefaultOutboundChainID",to))
		data.AddInt32("DefaultOutboundChainID",to);
}

}	// namespace Mail
}	// namespace Zoidberg
