#include <Alert.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include "imap_reader.h"

#define PRINT(a) printf a
#define _(a)	a

IMAP4Reader::IMAP4Reader(IMAP4Client *protocol, BPositionIO *dump_to, const char * seq_id) :
	slave(dump_to),
	message_id(seq_id),
	network(protocol),
	size(0),
	state(-1) {
		//-----do nothing, and do it well-----
	}
		
ssize_t	IMAP4Reader::ReadAt(off_t pos, void *buffer, size_t size) {
	off_t old_pos = slave->Position();
	void *test = malloc(2);
	free(test);
	while ((pos + size) > this->size) {
		if (state > 1)
			break;
		switch (state) {
			case -1:
				slave->Seek(0,SEEK_SET);
				FetchHeader(message_id,slave);
				slave->Seek(0,SEEK_END);
				slave->Write("\r\n\r\n",4);
				break;
			case 0:
				slave->Seek(0,SEEK_SET);
				slave->SetSize(0);
				FetchBody(message_id,slave);
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
	
	
ssize_t	IMAP4Reader::WriteAt(off_t pos, const void *buffer, size_t size) {
	ssize_t return_val;
	
	return_val = slave->WriteAt(pos,buffer,size);
	ResetSize();
	
	return return_val;
}

off_t IMAP4Reader::Seek(off_t position, uint32 seek_mode) {
	if (seek_mode == SEEK_END) {
		FetchBody(message_id,slave);
		state = 1;
	}
	return slave->Seek(position,seek_mode);
}

off_t IMAP4Reader::Position() const {
	return slave->Position();
}

void IMAP4Reader::ResetSize(void) {
	off_t old = slave->Position();
	
	slave->Seek(0,SEEK_END);
	size = slave->Position();
	
	slave->Seek(old,SEEK_SET);
}

status_t IMAP4Reader::FetchHeader(const char * index,BPositionIO *destination) {
	destination->Seek(SEEK_SET,0);
	return FetchInternal(index,destination," RFC822.HEADER");
}

status_t
IMAP4Reader::FetchBody(const char * index,BPositionIO *destination) {
	destination->Seek(SEEK_SET,0);
	return FetchInternal(index,destination," BODY[]");
}

status_t
IMAP4Reader::FetchInternal(const char * index,BPositionIO *destination, const char *fetch_what)
{
	// Check connection
	if(!network->IsAlive())
	{
		PRINT(("Re-connect\n"));
		status_t err = network->Reconnect();
		if(err != B_OK)
			return B_ERROR;
	}
	//
	
	BString cmd("UID FETCH ");
	cmd << index << fetch_what; //" BODY[]";
	
	//destination->SetSize(0);
	BString line;
	
	int32 pos,r=0,state,content_size=0;
	int32 session = network->fCommandCount+1;
	
	if( network->SendCommand(cmd.String()) == B_OK)
	{
		while(line.FindFirst("FETCH") == B_ERROR)
			r = network->ReceiveLine(line);
		if(r <=0)
			return B_ERROR;
			
		state = network->CheckSessionEnd(line.String(),session);
				
		switch(state)
		{
		case IMAP_SESSION_OK:
			return B_OK;
		case IMAP_SESSION_BAD:
			return B_ERROR;
		}
		// Get Last line
		while (1)
		{
			r = network->ReceiveLine(line);
			if (network->CheckSessionEnd(line.String(),session) != IMAP_SESSION_CONTINUED)
				break;
			
			destination->Write(line.String(),line.Length());
		}
		
		destination->SetSize(destination->Position() - 3);
		
		if(r <=0)
			return B_ERROR;
		state = network->CheckSessionEnd(line.String(),session);
			
		switch(state)
		{
		case IMAP_SESSION_OK:
			return B_OK;
		case IMAP_SESSION_BAD:
			return B_ERROR;
		}
	}
	return B_ERROR;
}

IMAP4Reader::~IMAP4Reader() {
	delete slave;
}
