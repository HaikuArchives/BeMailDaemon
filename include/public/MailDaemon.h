#ifndef ZOIDBERG_MAIL_DAEMON_H
#define ZOIDBERG_MAIL_DAEMON_H
/* Daemon - talking to the mail daemon
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


namespace Zoidberg {
namespace Mail {

status_t CheckMail(bool send_queued_mail = true);
status_t SendQueuedMail();
int32	CountNewMessages(bool wait_for_fetch_completion = false);

status_t QuitDaemon();

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_DAEMON_H */

