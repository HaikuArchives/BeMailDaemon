#ifndef MAIL_PROTOCOL_H
#define MAIL_PROTOCOL_H

#include <OS.h>

#include <MailAddon.h>

class StringList;

namespace Mail {
class ChainRunner;

class Protocol : public Filter
{
  public:
	Protocol(BMessage* settings);
	// Open a connection based on 'settings'.  'settings' will
	// contain a persistent uint32 ChainID field.  At most one
	// Protocol per ChainID will exist at a given time.
	
	virtual ~Protocol();
	// Close the connection and clean up.   This will be cal-
	// led after FetchMessage() or FetchNewMessage() returns
	// B_TIMED_OUT or B_ERROR, or when the settings for this
	// Protocol are changed.
	
	virtual status_t UniqueIDs() = 0;
	// Fill the protected member unique_ids with strings containing
	// unique ids for all messages present on the server. This al-
	// lows comparison of remote and local message manifests, so
	// the local and remote contents can be kept in sync.
	//
	// The ID should be unique to this Chain; if that means
	// this Protocol must add account/server info to differ-
	// entiate it from other messages, then that info should
	// be added before returning the IDs and stripped from the
	// ID for use in GetMessage() et al, below.
	//
	// Returns B_OK if this was performed successfully, or another
	// error if the connection has failed.
	
	virtual void PrepareStatusWindow(StringList *manifest)=0;
	// Prepare your status window by informing it of the number and
	// size of messages to be fetched. manifest contains the messages
	// already fetched, if any. unique_ids is gauranteed to be valid
	// when this routine is called. Note that you cannot simply do
	// *manifest ^ *unique_ids to get new messages, as manifest may
	// contain messages not on the server which were deleted by
	// another agent.
	
	virtual status_t GetNextNewUid
	(
		BString* out_uid,
		StringList *manifest,
		time_t timeout = B_INFINITE_TIMEOUT
	)=0;
	// Waits up to timeout microseconds until new mail is present
	// in the account.  It then stores the new message's unique id
	// (as above) in out_uid.
	//
	// Manifest contains a list of all messages currently on disk.
	// Skip them.
	//
	// Returns B_OK if the message ID is now available in out_uid,
	// B_TIMED_OUT if the timeout expired with no new messages, or
	// another error if the connection failed. Note that if, as in
	// POP3, the mailbox cannot gain new messages, and there is no
	// point in waiting for them, you should return B_TIMED_OUT
	// immediately.
	
	virtual status_t GetMessage(
		const char* uid,
		BPositionIO** out_file, BMessage* out_headers,
		BPath* out_folder_location
	)=0;
	// Downloads the message with id uid, writing the message's
	// RFC2822 contents to *out_file and storing any headers it
	// wants to add other than those from the message itself into
	// out_headers.  It may store a path (if this type of account
	// supports folders) in *out_folder_location.
	//
	// Returns B_OK if the message is now available in out_file,
	// B_NAME_NOT_FOUND if there is no message with id 'uid' on
	// the server, or another error if the connection failed.
	//
	// B_OK will cause the message to be stored and processed.
	// B_NAME_NOT_FOUND will cause appropriate recovery to be
	// taken (if such exists) but not cause the connection to
	// be terminated.  Any other error will cause anything writen
	// to be discarded and and the connection closed.
	
	// OBS:
	// The Protocol may replace *out_file with a custom (read-
	// only) BPositionIO-derived object that preserves the il-
	// lusion that the message is writen to *out_file, but in
	// fact only reads from the server and writes to *out_file
	// on demand.  This BPositionIO must guarantee that any
	// data returned by Read() has also been writen to *out_-
	// file.  It must return a read error if reading from the
	// network or writing to *out_file fails.
	//
	// The mail_daemon will delete *out_file before invoking
	// FetchMessage() or FetchNewMessage() again.
	
	virtual status_t DeleteMessage(const char* uid)=0;
	// Removes the message from the server.  After this, it's
	// assumed (but not required) that GetMessage(uid,...)
	// et al will fail with B_NAME_NOT_FOUND.
	
	//------MailFilter calls
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
	
  protected:
	StringList *unique_ids;
  	BMessage *settings;
	
  private:
  	friend class DeletePass;
  
  	StringList *manifest;
  	ChainRunner *parent;
};

// standard MailProtocols:
//
//   IMAP, POP3 - internet mail protocols
//   Internal - Messages sent by this system
//   Trash - Messages sent to the trash

class SimpleProtocol : public Protocol {
	public:
		SimpleProtocol(BMessage *settings, StatusView *view);
			//---Constructor. Simply call this in yours, and most everything will be handled for you.
		
		virtual void SetStatusReporter(StatusView *view) = 0;
			//---Stash view somewhere, and use the functions in status.h to report all your progress
			//---Notify it every 100 bytes or so
			
		virtual status_t Open(const char *server, int port, int protocol) = 0;
			//---server is an ASCII representation of the server that you are logging in to
			//---port is the remote port, -1 if you are to use default
			//---protocol is the protocol to use (defined by your add-on and useful for using, say, SSL) -1 is default
	
		virtual status_t Login(const char *uid, const char *password, int method) = 0;
			//---uid is the username provided
			//---likewise password
			//---method is the auth method to use, this works like protocol in Open
		
		virtual int32 Messages(void) = 0;
			//---return the number of messages waiting
		
		virtual status_t GetHeader(int32 message, BPositionIO *write_to) = 0;
			//---Retrieve the header of message <message> into <write_to>
		
		virtual status_t Retrieve(int32 message, BPositionIO *write_to) = 0;
			//---get message number <message>
			//---write your message to <write_to>
			
		virtual void Delete(int32 num) = 0;
			//---delete message number num
	
		virtual size_t MessageSize(int32 index) = 0;
			//---return the size in bytes of message number <index>
			
		virtual size_t MailDropSize(void) = 0;
			//---return the size of the entire maildrop in bytes
		
		//---Note that UniqueIDs is *not* covered for you, and is
		//   inherited unimplemented by SimpleMailProtocol
		
		//---------These implement hooks from up above---------
		//---------not user-servicable-------------------------
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
		virtual status_t DeleteMessage(const char* uid);
		virtual status_t InitCheck(BString* out_message = NULL);
		virtual void PrepareStatusWindow(StringList *manifest);
		virtual ~SimpleProtocol();
	
	protected:
		StatusView *status_view;
		status_t Init();
	
	private:
		status_t error;
		int32 last_message;
};

}

#endif // MAIL_PROTOCOL_H

