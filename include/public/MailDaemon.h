#ifndef MAIL_DAEMON_H
#define MAIL_DAEMON_H

namespace Mail {

status_t CheckMail(bool send_queued_mail = true);
status_t SendQueuedMail();
int32	CountNewMessages(bool wait_for_fetch_completion = false);

status_t QuitDaemon();

}

#endif // MAIL_DAEMON_H

