#include <BeBuild.h>

#include <stdio.h>
#include <malloc.h>

#include "MessageIO.h"
#include "MailProtocol.h"

MessageIO::MessageIO(SimpleMailProtocol *protocol, BPositionIO *dump_to, int32 seq_id) :
	slave(dump_to),
	message_id(seq_id),
	network(protocol),
	size(0),
	state(-1) {
		//-----do nothing, and do it well-----
	}
		
ssize_t	MessageIO::ReadAt(off_t pos, void *buffer, size_t size) {
	off_t old_pos = slave->Position();
	
	while ((pos + size) > this->size) {
		if (state > 1)
			break;
		switch (state) {
			case -1:
				slave->Seek(0,SEEK_SET);
				network->GetHeader(message_id,slave);
				slave->Seek(0,SEEK_END);
				slave->Write("\r\n\r\n",4);
				break;
			case 0:
				slave->Seek(0,SEEK_SET);
				slave->SetSize(0);
				network->Retrieve(message_id,slave);
				break;
		}
		
		state++;
		ResetSize();
	}
	
	if (old_pos < this->size)
		slave->Seek(old_pos,SEEK_SET);
	else
		slave->Seek(0,SEEK_END);
	
	return slave->ReadAt(pos,buffer,size);
}
	
	
ssize_t	MessageIO::WriteAt(off_t pos, const void *buffer, size_t size) {
	ssize_t return_val;
	
	return_val = slave->WriteAt(pos,buffer,size);
	ResetSize();
	
	return return_val;
}

off_t MessageIO::Seek(off_t position, uint32 seek_mode) {
	if (seek_mode == SEEK_END) {
		network->Retrieve(message_id,slave);
		state = 1;
	}
	return slave->Seek(position,seek_mode);
}

off_t MessageIO::Position() const {
	return slave->Position();
}

void MessageIO::ResetSize(void) {
	off_t old = slave->Position();
	
	slave->Seek(0,SEEK_END);
	size = slave->Position();
	
	slave->Seek(old,SEEK_SET);
}

MessageIO::~MessageIO() {
	delete slave;
}
