#ifndef NUMAIL_H
#define NUMAIL_H
//
// On that mediakit/mailkit schtick
// The goal of this mail kit is to create a framework for inter-
// acting with email servers to detect and download new mail, and
// support keeping a subset of the server synchronized with a sub-
// set of the data on the client.
//
#include <mail/E-mail.h>
#include <MailAddon.h>
#include <MailSettings.h>

#if _BUILDING_mail
#define _IMPEXP_MAIL
#else
#define _IMPEXP_MAIL
#endif

BMessenger*& mail_daemon_messenger_func();
#define mail_daemon_messenger (mail_daemon_messenger_func())

//
// Notification states:
//

//
// Use StartWatching(mail_daemon_messenger, CONSTANT) to monitor
// these states in the mail_daemon.  Where applicable, the notif-
// ication message will contain ChainID=id of the affected chain.
//
#define MD_GLOBAL_SETTINGS_CHANGED 'MDsg'
#define MD_CHAIN_SETTINGS_CHANGED  'MDsc' // has ChainID=id
#define MD_CHAIN_STOPPED           'MDcS' // has ChainID=id
#define MD_CHAIN_ACTIVATED         'MDcA' // has ChainID=id
#define MD_CHAIN_COMPLETED         'MDcC' // has ChainID=id
// message will contain entry='an entry_ref for the message'
// unless MD_DISCARD was returned by one of the Consumers.


//
// Other messages:
//

#define MD_ADD_CHAIN               'MDac'
// add the chain archived in the message field 'chain'
// to the list of active chains.  Iff the message has
// a field permenant=true, save it to disk (otherwise
// this chain will only be active until the mail_daemon
// is restarted or the chain is Remove()d).  The
// mail_daemon will reply with ChainID=the chain ID
// of the chain.

#define MD_REMOVE_CHAIN            'MDrc'
// remove the chain with id in the message's field
// ChainID

status_t WriteMessageFile(const BMessage& archive, const BPath& path, const char* name);

#endif
