#ifndef ZOIDBERG_MAIL_MESSAGE_IO_H
#define ZOIDBERG_MAIL_MESSAGE_IO_H
/* MessageIO - reading/writing messages (directly from the protocols)
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <DataIO.h>
#include <Path.h>
#include <Message.h>


namespace Zoidberg {
namespace Mail {

class SimpleProtocol;

class MessageIO : public BPositionIO {
	public:
		MessageIO(Mail::SimpleProtocol *protocol, BPositionIO *dump_to, int32 seq_id);
		~MessageIO();
		
		//----BPositionIO
		virtual	ssize_t		ReadAt(off_t pos, void *buffer, size_t size);
		virtual	ssize_t		WriteAt(off_t pos, const void *buffer, size_t size);
		
		virtual off_t		Seek(off_t position, uint32 seek_mode);
		virtual	off_t		Position() const;
		
	private:
		void ResetSize(void);
		
		BPositionIO *slave;
		int32 message_id;
		Mail::SimpleProtocol *network;
		
		size_t size;
		int state;
};

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_MAIL_MESSAGE_IO_H */
