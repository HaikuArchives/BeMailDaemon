#include <DataIO.h>
#include <String.h>
#include <File.h>
#include <Entry.h>
#include <Mime.h>
#include <ByteOrder.h>
#include <NodeInfo.h>

#include <malloc.h>

class _EXPORT SimpleMailAttachment;
class _EXPORT AttributedMailAttachment;

#include <MailAttachment.h>
#include <base64.h>
#include <NodeMessage.h>

//--------------SimpleMailAttachment-No attributes or awareness of the file system at large-----

SimpleMailAttachment::SimpleMailAttachment()
	: MailComponent(),
	_data(NULL),
	_we_own_data(false)
	{
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(BPositionIO *data)
	: MailComponent(),
	_data(data),
	_we_own_data(false)
	{
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(const void *data, size_t length)
	: MailComponent(),
	_data(new BMemoryIO(data,length)),
	_we_own_data(true)
	{
		SetEncoding(base64);
		AddHeaderField("Content-Disposition","Attachment");
	}


status_t SimpleMailAttachment::FileName(char *text) {
	BString name = HeaderField("Content-Type");
	int32 offset = name.IFindFirst("name=");
	if (offset < 0)
		return B_ERROR;
		
	name.Remove(0,offset);
	
	offset = name.IFindFirst(" ");
	if (offset >= 0)
		name.Truncate(offset);
	
	if (name.ByteAt(0) == '\"') {
		name.Remove(0,1);
		name.Truncate(name.IFindFirst("\""));
	}
	
	strcpy(text,name.String());
	return B_OK;
}
	
void SimpleMailAttachment::SetFileName(const char *name) {
	BString header;
	BMimeType mime;
	MIMEType(&mime);
	header << mime.Type() << "; name=\"" << name << "\"";
	
	AddHeaderField("Content-Type",header.String());
}

status_t SimpleMailAttachment::GetDecodedData(BPositionIO *data) {
	char buffer[255];
	size_t len = 255;
	_data->Seek(0,SEEK_SET);
	
	while ((len = _data->Read(buffer,255)) > 0)
		data->Write(buffer,len);
		
	return B_OK;
}

BPositionIO *SimpleMailAttachment::GetDecodedData() {
	return _data;
}

status_t SimpleMailAttachment::SetDecodedDataAndDeleteWhenDone(BPositionIO *data) {
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = true;
	
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

	char *cte = NULL; //--Content Transfer Encoding
	switch (_encoding) {
		case base64:
			cte = "base64";
			break;
		case quoted_printable:
		case no_encoding:
			// if there is really nothing to do here, we should
			// rearrange the "switch" statement to a simple "if"
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
AttributedMailAttachment::AttributedMailAttachment(BFile *file, bool delete_when_done) : MIMEMultipartContainer("++++++BFile++++++") {
	_data = new SimpleMailAttachment;
	AddComponent(_data);
	
	_attributes_attach = new SimpleMailAttachment;
	_attributes_attach->AddHeaderField("Content-Type","application/x-be_attribute; name=\"BeOS Attributes\"");
	
	AddHeaderField("Content-Type","multipart/x-bfile");
	
	if (file != NULL)
		SetTo(file,delete_when_done);
}

AttributedMailAttachment::AttributedMailAttachment(entry_ref *ref) {
	_data = new SimpleMailAttachment;
	AddComponent(_data);
	
	_attributes_attach = new SimpleMailAttachment;
	_attributes_attach->AddHeaderField("Content-Type","application/x-be_attribute; name=\"BeOS Attributes\"");
	
	AddHeaderField("Content-Type","multipart/x-bfile");
	
	if (ref != NULL)
		SetTo(ref);
}

void AttributedMailAttachment::SetTo(BFile *file, bool delete_file_when_done) {
	char buffer[512];
	_attributes << *file;
	
	BNodeInfo(file).GetType(buffer);
	_data->AddHeaderField("Content-Type",buffer);
	//---No way to get file name
	//_data->SetFileName(ref->name);
	
	if (delete_file_when_done)
		_data->SetDecodedDataAndDeleteWhenDone(file);
	else
		_data->SetDecodedData(file);
}
	
void AttributedMailAttachment::SetTo(entry_ref *ref) {
	char buffer[512];
	_attributes << BNode(ref);
	
	BFile *file = new BFile(ref,B_READ_ONLY);
	_data->SetDecodedDataAndDeleteWhenDone(file);
	BNodeInfo(file).GetType(buffer);
	_data->AddHeaderField("Content-Type",buffer);
	_data->SetFileName(ref->name);
}

void AttributedMailAttachment::SaveToDisk(BEntry *entry) {
	BString path = "/tmp/";
	char name[255] = "";
	_data->FileName(name);
	path << name;
	
	BFile file(path.String(),B_READ_WRITE | B_CREATE_FILE);
	file << _attributes;
	_data->GetDecodedData(&file);
	file.Sync();
	
	entry->SetTo(path.String());
}

void AttributedMailAttachment::SetEncoding(mail_encoding encoding) {
	_data->SetEncoding(encoding);
	_attributes_attach->SetEncoding(encoding);
}

mail_encoding AttributedMailAttachment::Encoding() {
	return _data->Encoding();
}

status_t AttributedMailAttachment::FileName(char *name) {
	return _data->FileName(name);
}
	
void AttributedMailAttachment::SetFileName(const char *name) {
	_data->SetFileName(name);
}

status_t AttributedMailAttachment::GetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		*node << _attributes;
		
	_data->GetDecodedData(data);
	return B_OK;
}

status_t AttributedMailAttachment::SetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		_attributes << *node;
		
	_data->SetDecodedData(data);
	return B_OK;
}

		
status_t AttributedMailAttachment::Instantiate(BPositionIO *data, size_t length) {
	status_t err;
	
	err = MIMEMultipartContainer::Instantiate(data,length);
	if (err < B_OK)
		return err;
		
	BMimeType type;
	MIMEMultipartContainer::MIMEType(&type);
	if (strcmp(type.Type(),"multipart/x-bfile") != 0)
		return B_BAD_TYPE;
		
	if (_data != NULL)
		delete _data;
	if (_attributes_attach != NULL)
		delete _attributes_attach;
	_attributes.MakeEmpty();
	
	//------This needs error checking badly------
	_data = (SimpleMailAttachment *)GetComponent(0);
	_attributes_attach = (SimpleMailAttachment *)GetComponent(1);
	
	int32		len;
	int32		index = 0;
	type_code	code;
	char		*name;
	len = ((BMallocIO *)(_attributes_attach->GetDecodedData()))->BufferLength();
	char *start = (char *)malloc(len);
	_attributes_attach->GetDecodedData()->ReadAt(0,start,len);
	
	while (index < len) {
		name = &start[index];
		index += strlen(name) + 1;
		memcpy(&code, &start[index], sizeof(type_code));
		code = B_BENDIAN_TO_HOST_INT32(code);
		index += sizeof(type_code);
		memcpy(&length, &start[index], sizeof(length));
		length = B_BENDIAN_TO_HOST_INT64(length);
		index += sizeof(length);
		swap_data(code, &start[index], length, B_SWAP_BENDIAN_TO_HOST);
		_attributes.AddData(name, code, &start[index], length);
		index += length;
	}
	
	free(start);
	
	return B_OK;
}
	
status_t AttributedMailAttachment::Render(BPositionIO *render_to) {
	BMallocIO *io = new BMallocIO;
	char *name;
	const void *data;
	void *allocd;
	type_code type, swap_typed;
	int64 length, swapped;
	ssize_t dataLen;
	for (int32 i = 0; _attributes.GetInfo(B_ANY_TYPE,i,&name,&type) == B_OK; i++) {
		_attributes.FindData(name,type,&data,&dataLen);
		io->Write(name,strlen(name) + 1);
		swap_typed = B_HOST_TO_BENDIAN_INT32(type);
		io->Write(&swap_typed,sizeof(type_code));
		length = dataLen;
		swapped = B_HOST_TO_BENDIAN_INT64(length);
		io->Write(&swapped,sizeof(int64));
		allocd = malloc(dataLen);
		memcpy(allocd,data,dataLen);
		swap_data(type, allocd, dataLen, B_SWAP_HOST_TO_BENDIAN);
		io->Write(allocd,dataLen);
		free(allocd);
	}
	
	_attributes_attach->SetDecodedDataAndDeleteWhenDone(io);
	
	status_t err = MIMEMultipartContainer::Render(render_to);
	
	return err;
}

status_t AttributedMailAttachment::MIMEType(BMimeType *mime) {
	return _data->MIMEType(mime);
}