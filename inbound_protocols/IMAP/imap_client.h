#ifndef __IMAP4CLIENT_H__
#define __IMAP4CLIENT_H__

#include <NetworkKit.h>
#include <String.h>
#include <Message.h>

#include <MailProtocol.h>

enum{
	IMAP_SESSION_CONTINUED = 0,
	IMAP_SESSION_OK,
	IMAP_SESSION_BAD	
};

//! IMAP4 client socket.
class IMAP4Client :public BNetEndpoint, public MailProtocol {
public:
		//!Constructor.
					IMAP4Client(BMessage *settings,StatusView *status);
					virtual status_t InitCheck(BString* out_message = NULL);
		//!Destructor.
	virtual			~IMAP4Client();
		//!Connect to IMAP4 server.
		virtual status_t	Open(const char* addr,int port,int protocol);
		//!Login to IMAP4 server.
		virtual status_t	Login(const char* login,const char* password, int method);
		//!Fetch mail list.
		virtual status_t UniqueIDs();
		virtual void PrepareStatusWindow(StringList *manifest);
		//! Select folder and returns how number of mail contains.
		int32		Select(const char* folder_name); 
		//!Mark mail as read.
		status_t	MarkAsRead(int32 index);
		//!Mark mail as delete.
		status_t 	MarkAsDelete(int32 index);
		virtual status_t DeleteMessage(const char* uid);
		//!Store new mail flags.
		status_t	Store(int32 index,const char* flags,bool add = true);
		//!Send noop command.
		status_t	Noop();
		//!Reconnect to IMAP4 server.
		status_t	Reconnect();
		//!Returns true if connection is alive.
		bool		IsAlive();
		//!Logout.
		void		Logout();
		
		virtual status_t GetNextNewUid
		(
			BString* out_uid,
			StringList *manifest,
			time_t timeout = B_INFINITE_TIMEOUT
		);
		virtual status_t GetMessage(
			const char* uid,
			BPositionIO** out_file, BMessage* out_headers,
			BPath* out_folder_location
		);
protected:
		status_t	SendCommand(const char* str);
		int32		ReceiveLine(BString &out);
		int32		CheckSessionEnd(const char* line,int32 session);
private:
		friend class IMAP4Reader;
		
		typedef	BNetEndpoint	_inherited;
		int32			fCommandCount;
		BString			fAddress;
		int				fPort;
		BString			fLogin;
		BString			fPassword;
		BString			fFolderName;
		time_t			fIdleTime;
		
		int32 num_messages;
		
		status_t error;
		
		BMessage *_settings;
		StatusView *_status;
		StringList *to_fetch;
};
#endif