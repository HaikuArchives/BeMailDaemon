class MailDaemon {
	public:
		
		static status_t CheckMail(bool send_queued_mail = true);
		static status_t SendQueuedMail();
		
		static status_t Quit();
};
