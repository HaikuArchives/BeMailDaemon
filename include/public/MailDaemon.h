class MailDaemon {
	public:
		
		static status_t CheckMail(bool send_queued_mail = true);
		static status_t SendQueuedMail();
		static int32	CountNewMessages(bool wait_for_fetch_completion = false);
		
		static status_t Quit();
};
