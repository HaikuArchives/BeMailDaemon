#ifndef ZOIDBERG_MAIL_PROTOCOL_H
#define ZOIDBERG_MAIL_PROTOCOL_H
/* Protocol - the base class for protocol filters
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <OS.h>

#include <MailAddon.h>


namespace Zoidberg {

class StringList;

namespace Mail {

class ChainRunner;

class Protocol : public Filter
{
  public:
	Protocol(BMessage* settings, ChainRunner *runner);
	// Open a connection based on 'settings'.  'settings' will
	// contain a persistent uint32 ChainID field.  At most one
	// Protocol per ChainID will exist at a given time.
	// The constructor of Mail::Protocol initializes manifest.
	// It is your responsibility to fill in unique_ids, *and
	// to keep it updated* in the course of whatever nefarious
	// things your protocol does.
	
	virtual ~Protocol();
	// Close the connection and clean up.   This will be cal-
	// led after FetchMessage() or FetchNewMessage() returns
	// B_TIMED_OUT or B_ERROR, or when the settings for this
	// Protocol are changed.
	
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
	
			void CheckForDeletedMessages();
	// You can call this to trigger a sweep for deleted messages.
	// Automatically called at the beginning of the chain.
	
	//------MailFilter calls
	virtual status_t ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, const char* io_uid
	);
	
  protected:
	StringList *manifest, *unique_ids;
  	BMessage *settings;
  	
	Mail::ChainRunner *runner;	
  private:
  	inline void error_alert(const char *process, status_t error);
	virtual void _ReservedProtocol1();
	virtual void _ReservedProtocol2();
	virtual void _ReservedProtocol3();
	virtual void _ReservedProtocol4();
	virtual void _ReservedProtocol5();

	friend class DeletePass;
	
	BHandler *trash_monitor;
	
	uint32 _reserved[4];
};

// standard MailProtocols:
//
//   IMAP, POP3 - internet mail protocols
//   Internal - Messages sent by this system

class SimpleProtocol : public Protocol {
	public:
		SimpleProtocol(BMessage *settings, Mail::ChainRunner *runner);
			//---Constructor. Simply call this in yours, and most everything will be handled for you.
			
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
		
		//---------These implement hooks from up above---------
		//---------not user-servicable-------------------------
		virtual status_t GetMessage(
			const char* uid,
			BPositionIO** out_file, BMessage* out_headers,
			BPath* out_folder_location
		);
		virtual status_t DeleteMessage(const char* uid);
		virtual status_t InitCheck(BString* out_message = NULL);
		virtual ~SimpleProtocol();

	protected:
		status_t Init();

	private:
		status_t error;
		int32 last_message;

		uint32 _reserved[5];
};

}	// namespace Mail
}	// namespace Zoidberg

#endif // ZOIDBERG_MAIL_PROTOCOL_H
