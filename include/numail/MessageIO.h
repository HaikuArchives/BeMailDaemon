#include <DataIO.h>
#include <Path.h>
#include <Message.h>

class SimpleMailProtocol;

class MessageIO : public BPositionIO {
	public:
		MessageIO(SimpleMailProtocol *protocol, BPositionIO *dump_to, int32 seq_id);
		
		//----BPositionIO
		virtual	ssize_t		ReadAt(off_t pos, void *buffer, size_t size);
		virtual	ssize_t		WriteAt(off_t pos, const void *buffer, size_t size);
		
		virtual off_t		Seek(off_t position, uint32 seek_mode);
		virtual	off_t		Position() const;
		
	private:
		void ResetSize(void);
		
		BPositionIO *slave;
		int32 message_id;
		SimpleMailProtocol *network;
		
		size_t size;
		int state;
};