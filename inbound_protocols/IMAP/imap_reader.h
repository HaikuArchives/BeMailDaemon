#include <DataIO.h>
#include <Path.h>
#include <Message.h>

#include "imap_client.h"

class IMAP4Reader : public BPositionIO {
	public:
		IMAP4Reader(IMAP4Client *protocol, BPositionIO *dump_to, const char *uid);
		~IMAP4Reader();
		
		//----BPositionIO
		virtual	ssize_t		ReadAt(off_t pos, void *buffer, size_t size);
		virtual	ssize_t		WriteAt(off_t pos, const void *buffer, size_t size);
		
		virtual off_t		Seek(off_t position, uint32 seek_mode);
		virtual	off_t		Position() const;
		
	private:
		void ResetSize(void);
		
		status_t FetchHeader(const char * index,BPositionIO *destination);
		status_t FetchBody(const char * index,BPositionIO *destination);
		status_t FetchInternal(const char * index,BPositionIO *destination,const char *fetch_what);
		
		BPositionIO *slave;
		const char * message_id;
		IMAP4Client *network;
		
		size_t size;
		int state;
};