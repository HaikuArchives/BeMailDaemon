#include <DataIO.h>
#include <String.h>

#include <malloc.h>

class _EXPORT SimpleMailAttachment;
class _EXPORT AttributedMailAttachment;

#include <MailAttachment.h>
#include <base64.h>

//--------------SimpleMailAttachment-No attributes or awareness of the file system at large-----

SimpleMailAttachment::SimpleMailAttachment() :
	_data(NULL),
	_we_own_data(false),
	MailComponent() {
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(BPositionIO *data) :
	_data(data),
	_we_own_data(false),
	MailComponent()  {
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(const void *data, size_t length) :
	_data(new BMemoryIO(data,length)),
	_we_own_data(true),
	MailComponent()  {
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}

status_t SimpleMailAttachment::GetDecodedData(BPositionIO *data) {
	char buffer[255];
	size_t len = 255;
	_data->Seek(0,SEEK_SET);
	
	while ((len = _data->Read(buffer,255)) > 0)
		data->Write(buffer,len);
		
	return B_OK;
}

status_t SimpleMailAttachment::SetDecodedData(BPositionIO *data) {
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = false;
	
	return B_OK;
}
	
status_t SimpleMailAttachment::SetDecodedData(const void *data, size_t length) {
	if (_we_own_data)
		delete _data;
	
	_data = new BMemoryIO(data,length);
	_we_own_data = true;
	
	return B_OK;
}
		
void SimpleMailAttachment::SetEncoding(mail_encoding encoding) {
	_encoding = encoding;
	
	char *cte; //--Content Transfer Encoding
	switch (_encoding) {
		case base64:
			cte = "base64";
			break;
	}
	
	AddHeaderField("Content-Transfer-Encoding",cte);
}

mail_encoding SimpleMailAttachment::Encoding() {
	return _encoding;
}

status_t SimpleMailAttachment::Instantiate(BPositionIO *data, size_t length) {
	//---------Massive memory squandering!---ALERT!----------
	
	off_t position = data->Position();
	MailComponent::Instantiate(data,length);
	
	length -= (data->Position() - position);
	
	BString encoding = HeaderField("Content-Transfer-Encoding");
	if (encoding.IFindFirst("base64") >= 0)
		_encoding = base64;
	else
		return B_BAD_TYPE;
	
	char *src = (char *)malloc(length);
	size_t size = length;
	
	if (_we_own_data)
		delete _data;
	
	size = data->Read(src,length);
	
	BMallocIO *buffer = new BMallocIO;
	buffer->SetSize(size); //-------8bit is *always* more efficient than an encoding, so the buffer will *never* be larger than before
	
	size = decode_base64((char *)(buffer->Buffer()),src,size);
	buffer->SetSize(size);
	
	free(src);
	
	_data = buffer;
	_we_own_data = true;
	
	return B_OK;
}

status_t SimpleMailAttachment::Render(BPositionIO *render_to) {
	//---------Massive memory squandering!---ALERT!----------
	if (_encoding != base64)
		return B_BAD_TYPE;

	_data->Seek(0,SEEK_END);
	size_t size = _data->Position();
	char *src = (char *)malloc(size);
	_data->Seek(0,SEEK_SET);
	
	size = _data->Read(src,size);
	
	char *dest = (char *)malloc(size*2); //--The encoded text will never be more than twice as large with base64
	
	size = encode_base64(dest,src,size);
	free(src);
	
	render_to->Write(dest,size);
	free(dest);
	
	return B_OK;
}

//-------AttributedMailAttachment--Awareness of bfs, sends attributes--
